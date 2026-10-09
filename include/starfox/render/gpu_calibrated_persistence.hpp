#pragma once
#include <cstdint>
#include <memory>
#include <string>
namespace starfox::render {
struct CalibratedPersistenceSettings {
    unsigned mode{},intensity{100},scale{1};
    bool models{true},world{};
    double seconds{};std::uint64_t epoch{};
    unsigned quality{2}; // Mode 3: independent CRT phosphor half-life preset.
};
// One instance per native eye. History is float RGB in authored 8-bit units,
// not a quantized copy of the final runtime texture. No production readback.
class GpuCalibratedPersistence {
public:
    GpuCalibratedPersistence();~GpuCalibratedPersistence();
    GpuCalibratedPersistence(const GpuCalibratedPersistence&)=delete;
    GpuCalibratedPersistence& operator=(const GpuCalibratedPersistence&)=delete;
    bool initialize(void* device,int color_format);
    // Caller retains the images/command through completion. A pending encode
    // cannot be overwritten. Commit only after successful presentation; abort
    // on cancellation/failure. Retrying a compositor wait never re-encodes.
    bool enqueue(void* command,void* source,void* ownership,void* destination,
        unsigned width,unsigned height,const CalibratedPersistenceSettings&);
    void commit() noexcept;
    // Reject only the pending candidate; keep the last accepted timeline.
    // The caller must still retain queued resources until GPU retirement.
    void cancel() noexcept;
    void discard() noexcept;
    void reset() noexcept;
    void release_device() noexcept;
    const std::string& status() const noexcept;
    std::uint64_t working_image_bytes() const noexcept; // retained float histories, no GPU calls
private:
    struct State;std::unique_ptr<State> state_;
};
}
