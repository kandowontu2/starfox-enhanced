#pragma once
// Host synchronization test doubles, not SDK/hardware/performance evidence.
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>

using s32 = std::int32_t;
using u64 = std::uint64_t;
using Result = std::int32_t;
using Handle = std::uint32_t;
inline constexpr Handle CUR_THREAD_HANDLE = 0xffff8000U;
inline constexpr bool R_FAILED(Result result) { return result < 0; }
enum ResetType { RESET_ONESHOT, RESET_STICKY };
struct LightLock { std::mutex value; };
struct LightEvent {
    std::mutex lock;
    std::condition_variable signal;
    bool ready{};
    ResetType reset{};
};
struct Thread_tag { std::thread value; };
using Thread = Thread_tag*;
using ThreadFunc = void (*)(void*);

Result APT_CheckNew3DS(bool*);
Result svcGetThreadPriority(s32*, Handle);
Thread threadCreate(ThreadFunc, void*, std::size_t, int, int, bool);
Result threadJoin(Thread, u64);
void threadFree(Thread);
void LightLock_Init(LightLock*);
void LightLock_Lock(LightLock*);
void LightLock_Unlock(LightLock*);
void LightEvent_Init(LightEvent*, ResetType);
void LightEvent_Signal(LightEvent*);
void LightEvent_Wait(LightEvent*);
