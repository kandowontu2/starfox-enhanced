#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
namespace starfox::render {
struct GpuCompositeOutput;
// Align current-frame material metadata with unjittered TAA/DLSS presentation.
// No history, CPU readback, or interpolation between unrelated surface normals.
class GpuTemporalSurfaces {
public:
    GpuTemporalSurfaces();~GpuTemporalSurfaces();
    bool enqueue(void* command,const GpuCompositeOutput& source,void* protected_pixels,
        std::array<float,2> jitter,GpuCompositeOutput& output,
        std::array<std::uint32_t,2> output_extent={});
    void release_device() noexcept;
    const std::string& status() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
struct GpuTemporalAaSettings {
    std::uint32_t width{},height{};std::uint64_t epoch{};
    float weight{.85f};
    std::array<float,4> projection{1,1,0,0},previous_z{0,0,1,0};
    // Native motion excludes sample jitter: current XY, previous XY in pixels.
    std::array<float,4> jitter{};
};
class GpuTemporalAa {
public:
    GpuTemporalAa();~GpuTemporalAa();
    // Same-device resident inputs. Caller submits/cancels the command; no readback.
    // commit() only after successful submission. discard() after cancellation.
    void* enqueue(void* device,void* command,void* color,void* depth,void* motion,
        void* packed_pixels,const GpuTemporalAaSettings&);
    void commit() noexcept;
    void discard() noexcept;
    void release_device() noexcept;
    const std::string& status() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
