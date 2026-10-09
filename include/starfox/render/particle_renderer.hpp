#pragma once

#include "starfox/render/framebuffer.hpp"
#include "starfox/render/software_renderer.hpp"
#include "starfox/simulation/particle_system.hpp"

#include <cstdint>

namespace starfox::render {

class ParticleRenderer {
public:
    struct OwnerFrame {
        std::vector<simulation::ParticleState> particles;
        simulation::ObjectHandle owner{};
        RenderPose pose{};
        double alpha{};
        std::uint8_t colour_base{112};
    };
    [[nodiscard]] static OwnerFrame prepare_owner(const simulation::ParticleSystem&,
        simulation::ObjectHandle,const RenderPose&,double alpha,std::uint8_t colour_base=112);
    static void draw_frame(const OwnerFrame&,Framebuffer&);
    void draw_owner(
        const simulation::ParticleSystem& particles,
        simulation::ObjectHandle owner,
        const RenderPose& owner_pose,
        double interpolation_alpha,
        Framebuffer& target,
        std::uint8_t colour_index_base = 7U * 16U) const;
private:
    static void draw_particles(std::span<const simulation::ParticleState>,simulation::ObjectHandle,
        const RenderPose&,double,Framebuffer&,std::uint8_t);
};

} // namespace starfox::render
