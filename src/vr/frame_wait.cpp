#include "starfox/vr/frame_wait.hpp"
#include <chrono>
#include <thread>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace starfox::vr {
FrameWait::FrameWait() {
#if defined(_WIN32)
    // Unsupported Windows versions simply use the portable fallback.
    timer_=CreateWaitableTimerExW(nullptr,nullptr,0x00000002,TIMER_MODIFY_STATE|SYNCHRONIZE);
#endif
}
FrameWait::~FrameWait() {
#if defined(_WIN32)
    if(timer_) CloseHandle(timer_);
#endif
}
void FrameWait::pause() {
#if defined(_WIN32)
    if(timer_) {
        LARGE_INTEGER due{};due.QuadPart=-10000; // Relative 1ms, in 100ns units.
        if(SetWaitableTimer(static_cast<HANDLE>(timer_),&due,0,nullptr,nullptr,FALSE)
            && WaitForSingleObject(static_cast<HANDLE>(timer_),20)==WAIT_OBJECT_0) return;
    }
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
}
