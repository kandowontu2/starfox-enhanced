#pragma once
#include "starfox/modern/sdl_source_scene.hpp"
#include "starfox/render/shadow_scene.hpp"
#include "starfox/vr/ray_lighting.hpp"
#include <functional>
#include <optional>
#include <span>
#include <unordered_map>
namespace starfox::modern {
// Ray-traced sun shadows for MODERN, caster side. SDL_GPU has no ray
// tracing pipelines, so each frame a compute pass (ray_shadow.hlsl) expands
// every physical compute-route world model into frame eye-space triangles and
// reduces one bounding box per object; the scene fragment shader then traces
// one ray per receiver pixel toward the sun against those boxes and their
// triangles (two levels, no BVH build). Hard shadows, no CPU readback.
// The same casters give coloured point lights (lights()) their visibility.
class SdlRayShadows {
public:
    SdlRayShadows()=default;
    ~SdlRayShadows();
    SdlRayShadows(const SdlRayShadows&)=delete;
    SdlRayShadows& operator=(const SdlRayShadows&)=delete;
    // After world.record_compute, outside any pass. `toward_light` is the
    // sun in native world axes (vr::source_shadow_environment); `caster`
    // filters object keys. False (with status) on failure; an empty scene is
    // success with object_count() 0.
    bool record(SDL_GPUDevice*,SDL_GPUCommandBuffer*,const SdlSourceScene& world,const vr::EyeCamera& eye,
        render::shadows::Vec3 toward_light,const std::function<bool(uint32_t)>& caster);
    // The shared lighting types (vr::ray_lighting.hpp).
    using PointLight=vr::RayPointLight;
    using Emitter=vr::RayEmitter;
    using EmitterKind=vr::RayEmitterKind;
    using BombFlash=vr::RayBombFlash;
    // After record(): this frame's lights from every world object (packets
    // and their keys), strongest first, at most `limit`, their reach scaled
    // by `radius_scale`; `tick` drives the engine glow's red/yellow flicker.
    std::vector<PointLight> lights(const vr::EyeCamera&,std::span<const vr::SourceComputeModel>,
        std::span<const vr::DrawPacket>,std::span<const uint32_t> handles,const EmitterKind&,std::optional<BombFlash>,
        std::size_t limit,float radius_scale=1.F,uint32_t tick=0) const;
    SDL_GPUBuffer* triangles() const noexcept {return triangles_;}
    SDL_GPUBuffer* objects() const noexcept {return objects_;}
    uint32_t object_count() const noexcept {return object_count_;}
    uint32_t triangle_count() const noexcept {return triangle_count_;}
    // Unit vector toward the sun in native eye space (+Y down, +Z forward).
    const render::shadows::Vec3& light() const noexcept {return light_;}
    void release() noexcept;
    const std::string& status() const noexcept {return status_;}
private:
    SDL_GPUDevice* device_{};
    SDL_GPUComputePipeline* pipeline_{};
    SDL_GPUBuffer* triangles_{};SDL_GPUBuffer* objects_{};
    uint32_t triangle_capacity_{},object_capacity_{},object_count_{},triangle_count_{};
    render::shadows::Vec3 light_{};
    std::unordered_map<uint32_t,uint32_t> object_index_; // Caster key -> objects() entry.
    SdlUploadBatch uploads_;
    std::string status_{"MODERN ray shadows not recorded"};
};
}
