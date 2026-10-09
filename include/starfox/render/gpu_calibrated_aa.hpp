#pragma once
#include <memory>
#include <string>
#include <cstdint>
namespace starfox::render {
constexpr bool calibrated_spatial_aa_supported(unsigned type) noexcept {
    return type<=2 || type==5; // FXAA, Sharp Edge, Soft Edge, upstream SMAA 1x.
}
constexpr bool calibrated_aa_supported(unsigned type) noexcept {
    return calibrated_spatial_aa_supported(type) || type==3 || type==4 || type==6;
}
constexpr bool calibrated_sample_extent_supported(unsigned w,unsigned h,unsigned factor) noexcept {
    // Overflow-safe working bounds, not a claim about currently free VRAM.
    return w && h && factor>=1 && factor<=4 && w<=16384/factor && h<=16384/factor
        && std::uint64_t(w)*h*factor*factor<=67108864;
}
// Resident native-eye AA. Raster ownership alpha is an explicit, opt-in AA
// eligibility mask, not colour opacity. UI, emissive and textured ink is exact.
// OFF copies exactly; unsupported algorithms fail, never become another AA.
class GpuCalibratedAa {
public:
    GpuCalibratedAa();~GpuCalibratedAa();
    GpuCalibratedAa(const GpuCalibratedAa&)=delete;
    GpuCalibratedAa& operator=(const GpuCalibratedAa&)=delete;
    bool initialize(void* device,int color_format);
    bool enqueue(void* command,void* source,void* ownership,void* destination,
        unsigned width,unsigned height,unsigned type,unsigned quality);
    // True 2x/3x/4x eye raster resolves. The two source images/receivers belong
    // to the caller: native ink is rasterized at the final extent and retained
    // exactly, while world/model samples are averaged in linear light. No CPU
    // image transfer and no enlargement of a previously rasterized mono image.
    bool enqueue_supersample(void* command,void* supersampled,void* high_ownership,
        void* native_ink,void* native_ownership,void* destination,
        unsigned width,unsigned height,unsigned factor);
    void release_device() noexcept;
    const std::string& status() const noexcept;
private:
    struct State;std::unique_ptr<State> state_;
};
}
