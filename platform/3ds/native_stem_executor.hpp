#pragma once
#include "starfox/audio/stem_executor.hpp"
#include <memory>

namespace starfox::platform::nintendo_3ds {
// One persistent, joined libctru worker on New-model application core 2.
// Old models and denied/failed core-2 allocation use the ordinary serial path;
// never steal NDSP's system core or alter source/DSP sample cadence.
// This owner and each borrowed task must outlive synchronous execute().
class NativeStemExecutor final : public audio::StemExecutor {
public:
    NativeStemExecutor();
    ~NativeStemExecutor() override;
    NativeStemExecutor(const NativeStemExecutor&) = delete;
    NativeStemExecutor& operator=(const NativeStemExecutor&) = delete;
    [[nodiscard]] bool parallel_available() const noexcept;
    void execute(audio::StemTask foreground, audio::StemTask background) override;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace starfox::platform::nintendo_3ds
