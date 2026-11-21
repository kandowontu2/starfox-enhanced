// 3D EXPLOSIONS as particles (vr::ExplosionParticles): emitters become
// anchored puffs plus free particles that outlive them, the same at any
// display rate, placed where SourceModels put the sprite.
#include "starfox/vr/explosion_particles.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
using namespace starfox;
using vr::ExplosionParticles;

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "explosion particle test failed: " << message << '\n';
        std::exit(1);
    }
}

vr::GameSceneSnapshot scene(std::uint64_t revision, std::int32_t camera_z = 0) {
    vr::GameSceneSnapshot s;
    s.revision = revision;
    s.scene_epoch = 1;
    s.camera.z = camera_z;
    s.view_matrix = {32767, 0, 0, 0, 32767, 0, 0, 0, 32767};
    s.display_brightness = 15;
    s.flow = simulation::GameFlowState::gameplay;
    return s;
}

vr::SourceExplosionEmitter emitter(render::ExplosionSpriteKind kind, std::uint8_t frame, std::uint8_t frames,
                                   std::array<double, 3> at, std::uint16_t handle = 7) {
    vr::SourceExplosionEmitter e;
    e.handle = handle;
    e.generation = 3;
    e.frame = {kind, frame, frames, 160.0};
    e.position = at;
    return e;
}

// Every vertex: the particle decal, eye-facing, finite, six per quad.
void check_packet(const vr::DrawPacket& packet, bool smoke) {
    const auto& vertices = packet.geometry.vertices;
    require(!vertices.empty() && vertices.size() % 6U == 0U, "quads of six vertices");
    for (const auto& v : vertices) {
        require(v.texture[3] == (4096U | 4U), "round eye-facing decal flags");
        require((v.texture[2] & 1U) == 1U && ((v.texture[2] >> 1U) & 7U) == (smoke ? 1U : 0U), "particle style");
        for (const float value : v.position) require(std::isfinite(value), "finite centre");
        for (const float value : v.billboard) require(std::isfinite(value), "finite corner");
        require(v.color[3] >= 0 && v.color[3] <= 1, "opacity in range");
    }
    if (smoke) {
        // Alpha blended: farthest first.
        for (std::size_t q = 6; q < vertices.size(); q += 6)
            require(vertices[q - 6].position[2] >= vertices[q].position[2], "smoke sorted back to front");
    }
}

void check_place() {
    ExplosionParticles::Particle p;
    p.origin = {10, 20, 30};
    p.velocity = {4, 0, 0};
    p.pull = {0, -1, 0};
    p.birth = 100;
    p.life = 10;
    p.size0 = 5;
    p.size1 = 15;
    p.drag = .5F;
    const auto start = ExplosionParticles::place(p, 100);
    require(start && std::abs((*start)[0] - 10) < 1e-9 && std::abs((*start)[3] - 5) < 1e-6, "starts at its origin and size");
    require(!ExplosionParticles::place(p, 99.9) && !ExplosionParticles::place(p, 110), "alive only during its life");
    const auto later = ExplosionParticles::place(p, 104);
    // Drag slows it: less than the undragged 16 units, toward 4 / 0.5 = 8.
    require(later && (*later)[0] > 10 && (*later)[0] < 18.1, "drag limits travel");
    require((*later)[1] < 20, "buoyancy lifts it (world Y down)");
}

void check_explosion_lifecycle() {
    ExplosionParticles particles;
    using Kind = render::ExplosionSpriteKind;
    const std::array<double, 3> at{100, -50, 2000};
    // Frames 0..7 over 16 ticks, then the sprite is gone.
    std::size_t peak = 0;
    for (std::uint64_t tick = 1; tick <= 16; ++tick) {
        const auto e = emitter(Kind::large, std::uint8_t((tick - 1) / 2), 8, at);
        particles.update(scene(tick - 1), scene(tick), 1.0, std::span(&e, 1));
        peak = std::max(peak, particles.free_particles());
        require(particles.anchored_particles() >= 6, "an anchored cluster per sprite");
        const auto packets = particles.build(false, vr::identity_matrix);
        require(packets.fire.has_value(), "fire while exploding");
        check_packet(*packets.fire, false);
        if (packets.smoke) check_packet(*packets.smoke, true);
        if (tick == 1) {
            // The anchored puffs sit around the sprite's centre.
            double sum = 0;
            for (const auto& v : packets.fire->geometry.vertices) sum += v.position[2];
            const double mean = sum / double(packets.fire->geometry.vertices.size());
            require(std::abs(mean - at[2]) < 160, "particles around the sprite");
        }
    }
    require(peak >= 28, "a burst of fire and sparks");
    // The sprite is gone: its smoke lives on, then fades away.
    particles.update(scene(16), scene(17), 1.0, {});
    require(particles.anchored_particles() == 0 && particles.free_particles() > 0, "smoke outlives the sprite");
    require(particles.build(false, vr::identity_matrix).smoke.has_value(), "lingering smoke drawn");
    for (std::uint64_t tick = 18; tick <= 60; ++tick) particles.update(scene(tick - 1), scene(tick), 1.0, {});
    require(particles.free_particles() == 0, "everything dies");
}

// Display rate does not change what is spawned or where it is.
void check_display_rate() {
    using Kind = render::ExplosionSpriteKind;
    ExplosionParticles once, often;
    const std::array<double, 3> at{0, 0, 1500};
    for (std::uint64_t tick = 1; tick <= 6; ++tick) {
        const auto e = emitter(Kind::small, std::uint8_t(tick > 3), 2, at, 9);
        for (const double alpha : {.25, .5, .75}) often.update(scene(tick - 1), scene(tick), alpha, std::span(&e, 1));
        often.update(scene(tick - 1), scene(tick), 1.0, std::span(&e, 1));
        once.update(scene(tick - 1), scene(tick), 1.0, std::span(&e, 1));
    }
    require(once.free_particles() == often.free_particles(), "same particles at any display rate");
    const auto a = once.build(false, vr::identity_matrix), b = often.build(false, vr::identity_matrix);
    require(a.fire && b.fire && a.fire->geometry.vertices == b.fire->geometry.vertices, "same placement at any display rate");
}

// Free particles stay in the world as the camera flies on.
void check_world_space() {
    using Kind = render::ExplosionSpriteKind;
    ExplosionParticles particles;
    const auto e = emitter(Kind::small, 0, 2, {0, 0, 1000});
    particles.update(scene(0, 0), scene(1, 0), 1.0, std::span(&e, 1));
    // The sprite vanishes; the camera moves 400 units forward.
    particles.update(scene(1, 0), scene(2, 400), 1.0, {});
    const auto packets = particles.build(false, vr::identity_matrix);
    require(packets.fire.has_value(), "burst still alive");
    double sum = 0;
    for (const auto& v : packets.fire->geometry.vertices) sum += v.position[2];
    const double mean = sum / double(packets.fire->geometry.vertices.size());
    require(mean < 1000 - 250 && mean > 1000 - 550, "particles keep their world place (" + std::to_string(mean) + ")");
}

// SMOKE's animation opens on a large-explosion frame: a flash, not a fireball.
void check_smoke_flash() {
    using Kind = render::ExplosionSpriteKind;
    ExplosionParticles explosion, smoke;
    auto bang = emitter(Kind::large, 0, 8, {0, 0, 1000});
    auto puff = bang;
    puff.source = vr::ExplosionSource::smoke;
    explosion.update(scene(0), scene(1), 1.0, std::span(&bang, 1));
    smoke.update(scene(0), scene(1), 1.0, std::span(&puff, 1));
    require(explosion.free_particles() >= 28, "an explosion bursts");
    require(smoke.free_particles() > 0 && smoke.free_particles() <= 4, "a smoke puff only flashes");
}

void check_scene_cut() {
    using Kind = render::ExplosionSpriteKind;
    ExplosionParticles particles;
    const auto e = emitter(Kind::large, 0, 8, {0, 0, 1000});
    particles.update(scene(0), scene(1), 1.0, std::span(&e, 1));
    require(particles.free_particles() > 0, "burst spawned");
    auto next = scene(2);
    next.scene_epoch = 2;
    particles.update(scene(1), next, 1.0, {});
    require(particles.free_particles() == 0, "a new scene drops the old particles");
}

} // namespace

int main() {
    check_place();
    check_explosion_lifecycle();
    check_display_rate();
    check_world_space();
    check_smoke_flash();
    check_scene_cut();
    std::cout << "explosion particle tests passed\n";
    return 0;
}
