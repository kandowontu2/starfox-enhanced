#pragma once
namespace starfox::vr {
// Sleeps, never spins. Windows uses a private high-resolution timer when available.
class FrameWait {
public:
    FrameWait();
    ~FrameWait();
    FrameWait(const FrameWait&)=delete;
    FrameWait& operator=(const FrameWait&)=delete;
    void pause();
private:
    void* timer_{};
};
}
