#pragma once

namespace starfox::audio {
// A task borrows its context until execute() returns or throws. Executors must
// finish every started task before either outcome: callers may then mix PCM,
// acknowledge ports, save/replace state, suspend or destroy the source owner.
struct StemTask {
    void* context{};
    void (*run)(void*){};
};
class StemExecutor {
public:
    virtual ~StemExecutor() = default;
    virtual void execute(StemTask foreground, StemTask background) = 0;
};
} // namespace starfox::audio
