#include "starfox/simulation/game_simulation.hpp"
#include <iostream>
#include <stdexcept>

int main() try {
    using namespace starfox::simulation;
    static_assert(platform_render_scale_limit==STARFOX_TEST_SCALE_LIMIT);
    for(unsigned scale=0;scale<render_scale_count;++scale) {
        const auto limited=GameSimulation::constrain_render_scale(static_cast<RenderScale>(scale));
        if(static_cast<unsigned>(limited)!=std::min(scale,platform_render_scale_limit-1U))
            throw std::runtime_error("Config/archive render scale escaped the platform limit");
    }
    for(bool backward:{false,true}) for(auto gpu:{GpuRenderer::accurate,GpuRenderer::fast}) {
        auto selected=GameSimulation::next_renderer_selection(RendererMode::gpu,gpu,backward,true);
        if(selected.first!=RendererMode::gpu || selected.second==gpu)
            throw std::runtime_error("Hardware-only renderer menu selected software or got stuck");
        selected=GameSimulation::next_renderer_selection(selected.first,selected.second,backward,true);
        if(selected.first!=RendererMode::gpu || selected.second!=gpu)
            throw std::runtime_error("Hardware-only GPU menu failed to wrap");
    }
    for(bool backward:{false,true}) {
        auto mode=RendererMode::software;auto gpu=GpuRenderer::accurate;
        unsigned software=0,accurate=0,fast=0;
        for(unsigned step=0;step<3;++step) {
            const auto selected=GameSimulation::next_renderer_selection(mode,gpu,backward,false);
            mode=selected.first;gpu=selected.second;
            software+=mode==RendererMode::software;
            accurate+=mode==RendererMode::gpu && gpu==GpuRenderer::accurate;
            fast+=mode==RendererMode::gpu && gpu==GpuRenderer::fast;
        }
        if(software!=1 || accurate!=1 || fast!=1 || mode!=RendererMode::software)
            throw std::runtime_error("Desktop renderer menu failed its three-way cycle");
    }
    std::cout<<"Platform "<<platform_render_scale_limit<<"x scale bound and two/three-way renderer cycles passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
