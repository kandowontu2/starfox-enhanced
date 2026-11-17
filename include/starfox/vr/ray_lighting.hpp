#pragma once
#include "starfox/assets/rom.hpp"
#include "starfox/render/shadow_geometry.hpp"
#include "starfox/vr/draw_packet.hpp"
#include "starfox/vr/game_scene.hpp"
#include "starfox/vr/source_models.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>
namespace starfox::vr {
// MODERN's ray-traced sun shadows and coloured lights, the parts free of any
// graphics API, shared by the flat renderer (modern::SdlRayShadows,
// modern::ModernRuntime) and the PCVR app (VulkanRayShadows). Moved from
// those files. "Native" axes are the cartridge's: +Y down, +Z forward, in
// source units (256 per metre).

// Emitter classes: each kind of object that gives off light.
// EmitterBoost: the cartridge's boost flame (BOOSTSHAPE) behind an Arwing.
enum RayEmitter : unsigned {EmitterNone,EmitterLaser,EmitterExplosion,EmitterPlasma,EmitterEngine,EmitterBomb,EmitterArwingLaser,
    EmitterBoost};
using RayEmitterKind=std::function<unsigned(uint32_t key)>;
// The cartridge's light sources and non-occluders, by shape and strategy.
class RayEmitters {
public:
    explicit RayEmitters(const assets::SymbolMap&);
    // The emitter class of a world object (packet key) in this snapshot.
    unsigned kind(const GameSceneSnapshot&,uint32_t key) const;
    // Whether the object casts shadows: shots, plasma, bombs and the boost
    // flame are light, not matter; muzzle flashes and the reticle are
    // indicators (vr::SourceRayPolicy).
    bool caster(const GameSceneSnapshot&,uint32_t key) const;
    // Corneria's launch tunnel (BG_1_1I): a wireframe hangar over black,
    // with nothing for a shadow to land on.
    bool shadowless(const GameSceneSnapshot&) const noexcept;
private:
    std::unordered_set<uint16_t> beam_shapes_,plasma_shapes_,ship_shapes_,bomb_shapes_,boost_shapes_;
    std::unordered_set<uint32_t> arwing_shot_strategies_,bomb_strategies_;
    std::optional<uint16_t> intro_background_;
    uint32_t flash_strategy_{};
    uint16_t reticle_shape_{};
};
// The sun at `elevation` degrees above the horizon (straight overhead in
// corridors, tunnels and the boss room), toward it in native world axes:
// GPU FAST's and VR's direction (vr::source_shadow_environment). None when
// the frame has no shadow environment.
std::optional<render::shadows::Vec3> ray_sun(const GameSceneSnapshot& previous,const GameSceneSnapshot& current,
    double alpha,double elevation);
// SHADOW SOFTNESS (0 hard .. 3 high): GPU FAST's sun disc radius and the
// fixed rays across it.
struct RaySoftness {float radius{};unsigned samples{1};};
RaySoftness ray_softness(int level) noexcept;
// Whether the eye's view is a rigid transform (rotation and translation).
bool ray_rigid(const EyeCamera&) noexcept;
// A world direction in the eye's native axes.
render::shadows::Vec3 ray_eye_direction(const EyeCamera&,render::shadows::Vec3) noexcept;
// A model's placement as three rows from its model units to the eye's
// native axes (ray_shadow.hlsl Settings rows).
std::optional<std::array<std::array<float,4>,3>> ray_model_rows(const EyeCamera&,const Matrix4& placement,double units);
// A coloured point light (laser, plasma ball, explosion, engine glow or
// smart bomb) in native eye space, source units.
// `object` is its own entry in the casters' object table (never shadows its
// own light), or ~0. `hardness` keeps full brightness out to that fraction of
// the radius, then falls off steeply. A `projected` light is a pool on
// background surfaces only (ground, corridor, room), centred where the sun
// casts its position, `aspect` times longer along `axis`, fading out
// `fade_height` above them: an engine's glow under its ship.
struct RayPointLight {
    std::array<float,3> position{};float radius{};std::array<float,3> colour{};uint32_t object{~0U};
    std::array<float,3> axis{1,0,0};float aspect{1};float hardness{};bool projected{};float fade_height{};
    bool background_only{}; // An ordinary light that skips models (a boost flame on floor and walls).
    // Its own object may block it (an engine's glow cut off by the hull).
    bool self_shadowed{};
    // Projected lights: traced from the ground to a point this far up the
    // sun's rays from the light (native units). Larger = the cut follows the
    // sun's shadow more closely; smaller = it shrinks toward the light, which
    // then reaches into the shadow.
    float lift{};
    // Projected lights: the pool's radius at fade_height, as a multiple of
    // its radius on the ground (it widens and dims as the light climbs).
    float spread{1};
};
// The smart bomb's detonation circle (CircleEffectState), screen space.
struct RayBombFlash {float centre_x{},centre_y{},radius{};std::array<float,3> colour{};};
std::optional<RayBombFlash> ray_bomb_flash(const GameSceneSnapshot&) noexcept;
// This frame's lights from every world object (packets and their keys),
// strongest first, at most `limit`, their reach scaled by `radius_scale`;
// `tick` drives the engine glow's red/yellow flicker. `light` is toward the
// sun in the eye's native axes; `objects` maps caster keys to their entries
// in the object table.
std::vector<RayPointLight> ray_point_lights(const EyeCamera&,const render::shadows::Vec3& light,
    const std::unordered_map<uint32_t,uint32_t>& objects,std::span<const SourceComputeModel>,
    std::span<const DrawPacket>,std::span<const uint32_t> handles,const RayEmitterKind&,std::optional<RayBombFlash>,
    std::size_t limit,float radius_scale=1.F,uint32_t tick=0);
}
