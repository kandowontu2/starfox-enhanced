#pragma once
#include <memory>
#include <span>
#include <string>
#include <optional>
#include <cstdint>
#include "starfox/vr/eye_camera.hpp"
namespace starfox::render {
// Map a single-sample ray/depth consumer's pixel centres to ONE standard
// hardware raster sample. Eye pose/view/effects remain exact; only projection
// centre shifts. Never use this camera to rerasterize the multisample scene.
std::optional<vr::EyeCamera> calibrated_msaa_sample_camera(const vr::EyeCamera&,
    unsigned width,unsigned height,unsigned count,unsigned index) noexcept;
// Bound resident eye images/history; not a claim about currently free VRAM.
constexpr std::optional<std::uint64_t> calibrated_msaa_working_bytes(unsigned w0,unsigned h0,unsigned w1,unsigned h1,
    unsigned count,bool surfaces,unsigned histories,bool rays,bool fog=false,bool blur=false,bool water_layers=false) noexcept {
    if(!w0 || !h0 || !w1 || !h1 || w0>16384 || h0>16384 || w1>16384 || h1>16384
        || (count!=2 && count!=4 && count!=8) || histories>3) return {};
    const std::uint64_t pixels=std::uint64_t(w0)*h0+std::uint64_t(w1)*h1;
    // Raw colour/receiver/D32/optional surfaces and one final plane per sample;
    // shared extraction/scratch/native ink; two float histories per trail.
    // Ray output/meters have a conservative allowance; AS/geometry are separate.
    // Joint particle shutter includes two per-pixel candidate-mask words.
    // Ownership-only liquid styles still retain both float merge targets;
    // the raw float target is reusable, not a per-hardware-sample attachment.
    const std::uint64_t per_pixel=36+((surfaces || water_layers)?16:0)+(blur?172:0)+(water_layers?20:0)
        +count*(16+(surfaces?16:0)+histories*32+(rays?24:0)+4+(fog?16:0)+(water_layers?40:0));
    // Every fog sample retains its integral and a worst-case 16-MiB hierarchy
    // until submission completes. Geometry/driver storage remains separate.
    return pixels*(per_pixel+(blur?32*count:0))+(fog?std::uint64_t(count)*32*1024*1024:0);
}
constexpr bool calibrated_msaa_extent_supported(unsigned w0,unsigned h0,unsigned w1,unsigned h1,
    unsigned count,bool surfaces,unsigned histories,bool rays,bool fog=false,bool blur=false,bool water_layers=false) noexcept {
    const auto bytes=calibrated_msaa_working_bytes(w0,h0,w1,h1,count,surfaces,histories,rays,fog,blur,water_layers);
    return bytes && *bytes<=4ULL*1024*1024*1024;
}
// Resident per-sample transport. Sources are matching, retained, shader-readable
// multisample colour attachments; targets are matching single-sample textures.
// Texture creation requires the opted-in pinned SDL desktop capability/property
// in sdl_multisample.h. No CPU image transfer, queue submission or wait here.
class GpuCalibratedMsaa {
public:
    GpuCalibratedMsaa();~GpuCalibratedMsaa();
    GpuCalibratedMsaa(const GpuCalibratedMsaa&)=delete;
    GpuCalibratedMsaa& operator=(const GpuCalibratedMsaa&)=delete;
    bool initialize(void* device,int color_format);
    // Exact colour, discrete ownership and optional normal/depth at ONE sample.
    // Sources/targets never alias. Optional surfaces/motion must be paired.
    // Motion requires surfaces and is extracted from the SAME hardware sample,
    // not reconstructed from the resolved centre or a rerasterized eye.
    bool enqueue_sample(void* command,void* color,void* receiver,void* surfaces,
        void* sample_color,void* sample_receiver,void* sample_surfaces,
        unsigned width,unsigned height,unsigned count,unsigned index,
        void* motion=nullptr,void* sample_motion=nullptr);
    // Average fully enhanced independent sample planes in linear light. No
    // ownership/depth average. Optional paired native colour/class planes keep
    // protected centre-sample ink exact; ordered native UI is drawn afterwards.
    bool enqueue_resolve(void* command,std::span<void* const> sample_colors,void* destination,
        unsigned width,unsigned height,void* native_ink=nullptr,void* native_ownership=nullptr);
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    struct State;std::unique_ptr<State> state_;
};
}
