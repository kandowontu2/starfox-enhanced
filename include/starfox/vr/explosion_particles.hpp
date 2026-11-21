#pragma once
#include "starfox/vr/draw_packet.hpp"
#include "starfox/vr/source_models.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>
namespace starfox::vr {
// 3D EXPLOSIONS as particles (MODERN, flat and headset). SourceModels leaves
// the cartridge's fire, smoke and explosion sprites out and lists them as
// emitters (SourceModelPackets::explosions); this turns them into soft,
// camera-facing particles in the cartridge's world:
// - an anchored cluster per sprite, riding with it and sized and coloured by
//   its animation frame, so the cartridge still decides where, how big and
//   for how long (the sprite's silhouette becomes a 3D volume of puffs);
// - free particles it throws off (an explosion's fireball burst and sparks,
//   the smoke it leaves, a fireball's trail, a fire's flames), which live on
//   in world space after the sprite is gone.
// Free particles are spawned once per source tick (seeded by the emitter and
// the tick) and moved analytically by age, so any display rate and both eyes
// see the same particles. Graphics-API free: the renderers draw `smoke`
// (alpha blended, back to front) then `fire` (added), depth-tested without
// writing depth, after the world.
class ExplosionParticles {
public:
    void reset() noexcept;
    // Once per presented frame, with the same snapshots and fraction as
    // SourceModels::assemble_world_interpolated (whose emitters these are).
    void update(const GameSceneSnapshot& previous,const GameSceneSnapshot& current,double alpha,
        std::span<const SourceExplosionEmitter> emitters);
    // The current particles as quads for the scene shader's particle decal
    // (texture.w 4096|4, texture.z style; scene.hlsl). `model` places source
    // camera space (game_model_matrix at the origin, then the seat).
    struct Packets {std::optional<DrawPacket> smoke,fire;};
    [[nodiscard]] Packets build(bool srgb,const Matrix4& model) const;
    // Exposed for tests.
    [[nodiscard]] std::size_t free_particles() const noexcept {return particles_.size();}
    [[nodiscard]] std::size_t anchored_particles() const noexcept {return anchored_.size();}
    [[nodiscard]] double time() const noexcept {return time_;}
    enum class Style : std::uint8_t {fire,smoke,boost}; // boost: drawn with fire, in blue
    struct Particle {
        std::array<double,3> origin{};   // Cartridge world (Y down) at birth.
        std::array<double,3> velocity{}; // World units per source tick.
        std::array<double,3> pull{};     // Constant acceleration (buoyancy).
        double birth{},life{};           // Source ticks.
        float size0{},size1{};           // Radius at birth and at death.
        float drag{};                    // Per tick (exponential).
        float heat{};                    // Fire: start of the colour ramp; smoke: darkness.
        float opacity{};
        Style style{};
        std::uint8_t seed{};
    };
    // A particle's world position and radius at `time`; nullopt once dead.
    [[nodiscard]] static std::optional<std::array<double,4>> place(const Particle&,double time) noexcept;
private:
    struct Track {
        double first_seen{},last_seen{};
        std::array<double,3> world{},velocity{};
        std::uint8_t frame{};
        bool burst{},looped{};
        std::uint64_t spawned_tick{};
    };
    struct Anchored {Particle particle;std::array<double,3> world{};};
    void spawn(std::uint64_t key,Track&,const SourceExplosionEmitter&,std::uint64_t tick,double birth);
    std::unordered_map<std::uint64_t,Track> tracks_;
    std::vector<Particle> particles_;
    std::vector<Anchored> anchored_;
    double time_{};
    std::optional<std::uint64_t> epoch_,revision_;
    // The interpolated source camera of the last update (world -> camera).
    std::array<double,3> camera_{};
    std::array<double,9> view_{};
    float brightness_{1};
};
}
