#pragma once
#include "starfox/modern/sdl_scene_packets.hpp"
#include "starfox/render/gpu_scalefx.hpp"
#include <array>
#include <string>
namespace starfox::modern {
// The desktop's 2D filter (EDGE, xBRZ, sharp bilinear, CRT, ScaleFX) for
// MODERN's flat 2D layers. Each layer is drawn at source resolution (one
// texel per SNES pixel, transparent where empty), reconstructed at the
// output scale with effects_compute.hlsl's rules, and composited over the
// frame in the layer's place.
class SdlLayerFilter {
public:
    static constexpr unsigned layer_count=2;
    SdlLayerFilter()=default;
    ~SdlLayerFilter();
    SdlLayerFilter(const SdlLayerFilter&)=delete;
    SdlLayerFilter& operator=(const SdlLayerFilter&)=delete;
    bool initialize(SDL_GPUDevice*,SDL_GPUTextureFormat colour,SDL_GPUTextureFormat depth);
    // Source extent (target / factor) and filter factor for this frame;
    // recreates the layer textures when they change.
    bool resize(unsigned source_width,unsigned source_height,unsigned factor);
    // Pipelines for drawing into a layer (single sample, no depth test).
    SdlScenePipelines& pipelines() noexcept {return pipelines_;}
    // A render pass on layer `index`'s source texture, cleared transparent.
    SDL_GPURenderPass* begin_layer(SDL_GPUCommandBuffer*,unsigned index);
    // After that pass ends, outside any pass: filter the layer (`filter` is
    // render::TwoDFilter's value).
    bool filter(SDL_GPUCommandBuffer*,unsigned index,unsigned filter);
    // Inside the frame's pass (colour/depth formats as initialized, `samples`
    // as the pass): draw the filtered layer over the whole target.
    bool composite(SDL_GPUCommandBuffer*,SDL_GPURenderPass*,unsigned index,SDL_GPUSampleCount samples);
    unsigned source_width() const noexcept {return width_;}
    unsigned source_height() const noexcept {return height_;}
    void release() noexcept;
    const std::string& status() const noexcept {return status_;}
private:
    void release_textures() noexcept;
    SDL_GPUDevice* device_{};
    SDL_GPUTextureFormat colour_{},depth_format_{};
    SdlScenePipelines pipelines_;
    SDL_GPUComputePipeline* compute_{};
    SDL_GPUShader* vertex_{};SDL_GPUShader* fragment_{};
    SDL_GPUGraphicsPipeline* composite_{};
    SDL_GPUSampleCount composite_samples_{SDL_GPU_SAMPLECOUNT_1};
    SDL_GPUSampler* sampler_{};
    SDL_GPUBuffer* empty_{};
    std::array<SDL_GPUTexture*,layer_count> source_{},output_{};
    SDL_GPUTexture* depth_{};
    unsigned width_{},height_{},factor_{};
    std::array<float,layer_count> filled_{1,1};
    render::GpuScaleFx scalefx_;
    std::string status_{"MODERN layer filter not initialized"};
};
}
