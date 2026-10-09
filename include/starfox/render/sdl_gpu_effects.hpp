#pragma once
#include "starfox/render/gpu_effects.hpp"
#include "starfox/render/gpu_composite.hpp"
namespace starfox::render {
class SdlGpuEffects {
public:
    SdlGpuEffects();
    ~SdlGpuEffects();
    // Same-device, already-submitted queue consumers only. Cycled CPU staging
    // can encode ahead with bounded fence retirement; readback/destruction wait.
    void set_ordered_queue_reuse(bool enabled) noexcept {ordered_queue_reuse_=enabled;}
    bool apply(void* device,const Framebuffer&,std::vector<std::uint8_t>&,const GpuEffectSettings&);
    // Consumes composition directly on its device. CPU frame supplies only
    // dimensions/draw scale; no RGBA, index, tag or surface upload is needed.
    bool apply_resident(const GpuCompositeOutput&,const Framebuffer&,
        std::vector<std::uint8_t>&,const GpuEffectSettings&);
    bool readback(std::vector<std::uint8_t>&);
    void release_device() noexcept;
    const std::string& status() const;
    // Texel payload only, excluding driver alignment/metadata and buffers.
    std::uint64_t texture_payload_bytes() const noexcept;
    // Aligned staging bytes for the last apply; excludes uniform push data.
    std::uint32_t last_staging_upload_bytes() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool ordered_queue_reuse_{};
};
}
