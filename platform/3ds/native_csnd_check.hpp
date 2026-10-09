#pragma once
#include <memory>

namespace starfox::platform::nintendo_3ds {
// Asset-free service/stereo check only. Does not replace the player PCM sink
// or promise seamless streaming/pause support. Owns immutable one-shot memory.
class NativeCsndCheck {
public:
    NativeCsndCheck();
    ~NativeCsndCheck();
    NativeCsndCheck(const NativeCsndCheck&)=delete;
    NativeCsndCheck& operator=(const NativeCsndCheck&)=delete;
    void start(); // Rejects restarting active DMA; release owner before retry.
    [[nodiscard]] bool finished(); // Both channels must actually be inactive.
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace starfox::platform::nintendo_3ds
