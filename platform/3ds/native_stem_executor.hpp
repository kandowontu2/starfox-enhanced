#pragma once
#include "starfox/audio/stem_executor.hpp"
#include <memory>

namespace starfox::platform::nintendo_3ds {
// Persistent, joined libctru worker on New-model core 2 by default. An optional
// second owner uses only an APT-granted core-1 budget, restoring it after join.
// Unsupported models or denied allocation/budget retain the serial path.
// Neither role changes source/DSP sample cadence or NDSP ownership.
// This owner and each borrowed task must outlive synchronous execute().
class NativeStemExecutor final : public audio::StemExecutor {
public:
    enum class Role { application, budgeted_effects };
    explicit NativeStemExecutor(Role role=Role::application);
    ~NativeStemExecutor() override;
    NativeStemExecutor(const NativeStemExecutor&) = delete;
    NativeStemExecutor& operator=(const NativeStemExecutor&) = delete;
    [[nodiscard]] bool parallel_available() const noexcept;
    void execute(audio::StemTask foreground, audio::StemTask background) override;
    bool begin(audio::StemTask background) override;
    void finish() override;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace starfox::platform::nintendo_3ds
