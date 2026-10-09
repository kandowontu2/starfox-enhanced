#pragma once
#include <cstdint>
#include <memory>
#include <string>
namespace starfox::render {
struct CalibratedExposureSettings {
    unsigned quality{};
    float seconds{};
    std::uint64_t epoch{};
    bool paused{};
};
// Resident, trimmed log-luminance meter for calibrated eye images. Keep one
// owner per eye; native UI/emissive ink never meters or receives exposure.
class GpuCalibratedExposure {
public:
    GpuCalibratedExposure();~GpuCalibratedExposure();
    GpuCalibratedExposure(const GpuCalibratedExposure&)=delete;
    GpuCalibratedExposure& operator=(const GpuCalibratedExposure&)=delete;
    bool initialize(void* device,int color_format);
    // The command/images and this owner outlive GPU completion. A pending
    // encode cannot be replaced. Runtime waits retry presentation, not meters.
    bool enqueue(void* command,void* source,void* ownership,void* destination,
        unsigned width,unsigned height,const CalibratedExposureSettings&);
    void commit() noexcept; // Only after successful complete presentation.
    void cancel() noexcept; // Reject pending meter; preserve accepted stops/time.
    void discard() noexcept; // Cancellation/failure resets trusted history.
    void release_device() noexcept;
    const std::string& status() const noexcept;
    std::uint64_t working_image_bytes() const noexcept; // retained meter images, no GPU calls
private:
    struct State;std::unique_ptr<State> state_;
};
}
