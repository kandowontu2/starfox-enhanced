#pragma once
#include <chrono>
#include <cstdint>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#pragma push_macro("near")
#pragma push_macro("far")
#include <windows.h>
#pragma pop_macro("far")
#pragma pop_macro("near")
#elif defined(__unix__) || defined(__APPLE__)
#include <time.h>
#endif

namespace starfox::render {
// Opt-in diagnostics only. CPU time is executing time of this thread, not
// elapsed time or CPU frequency-converted cycles. Other/driver worker threads
// are excluded. Short individual deltas may be quantized by the OS; aggregate
// many annotated model draws rather than interpreting a zero as free work.
struct ModelThreadCpuClock {
    std::uint64_t ticks{}; // 100 ns, kernel + user on Windows.
    bool valid{};
};
inline const char* model_thread_cpu_provider() noexcept {
#if defined(_WIN32)
    return "win32-thread-kernel-user";
#elif defined(CLOCK_THREAD_CPUTIME_ID)
    return "posix-thread-cputime";
#else
    return "unsupported";
#endif
}
inline ModelThreadCpuClock model_thread_cpu_now(bool enabled) noexcept {
    if(!enabled) return {}; // No OS clock query on the ordinary render path.
#if defined(_WIN32)
    FILETIME created{},exited{},kernel{},user{};
    if(!GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user)) return {};
    const auto value=[](FILETIME v) {return (std::uint64_t(v.dwHighDateTime)<<32)|v.dwLowDateTime;};
    return {value(kernel)+value(user),true};
#elif defined(CLOCK_THREAD_CPUTIME_ID)
    timespec value{};
    if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)!=0 || value.tv_sec<0 || value.tv_nsec<0) return {};
    return {std::uint64_t(value.tv_sec)*10000000+std::uint64_t(value.tv_nsec)/100,true};
#else
    return {};
#endif
}
inline ModelThreadCpuClock model_thread_cpu_delta(ModelThreadCpuClock first,ModelThreadCpuClock last) noexcept {
    if(!first.valid || !last.valid || last.ticks<first.ticks) return {};
    return {last.ticks-first.ticks,true};
}
struct ModelHostClockSample {
    std::chrono::steady_clock::time_point wall{};
    ModelThreadCpuClock cpu{};
};
inline ModelHostClockSample model_host_clock_now(bool enabled,bool cpu) noexcept {
    if(!enabled) return {};
    return {std::chrono::steady_clock::now(),model_thread_cpu_now(cpu)};
}
}
