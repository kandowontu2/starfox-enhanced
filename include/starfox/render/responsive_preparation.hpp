#pragma once

#include <chrono>
#include <atomic>
#include <future>
#include <thread>
#include <type_traits>
#include <utility>

namespace starfox::render {

// Only resource preparation belongs on this worker. Recording/submitting GPU
// commands, rendering, and presentation remain on their owning thread. The
// caller stays in this function until the job is joined, keeping every borrowed
// create-info, shader and device alive (including when the job throws).
struct PreparationEvents {
    void (*pump)(void*) noexcept{};
    void* user{};
    unsigned jobs{},pumps{};
};
inline thread_local PreparationEvents* preparation_events{};

class ScopedPreparationEvents {
    PreparationEvents* previous_{};
public:
    explicit ScopedPreparationEvents(PreparationEvents* events) noexcept
        :previous_(std::exchange(preparation_events,events)) {}
    ~ScopedPreparationEvents() {preparation_events=previous_;}
    ScopedPreparationEvents(const ScopedPreparationEvents&)=delete;
    ScopedPreparationEvents& operator=(const ScopedPreparationEvents&)=delete;
};

template<class Job> auto responsive_prepare(Job&& job) -> std::invoke_result_t<Job> {
    auto* events=preparation_events;
    if(!events || !events->pump) return std::forward<Job>(job)();
    std::packaged_task<std::invoke_result_t<Job>()> task(std::forward<Job>(job));
    auto result=task.get_future();
    std::atomic_bool finished{};
    // No context is inherited by the worker. Pump callbacks also temporarily
    // suspend it, so an event watch cannot recursively launch another job.
    // Use the portable thread API rather than jthread: older Apple libc++
    // releases support our C++20 build but do not yet provide jthread.
    struct JoinedWorker {
        std::thread thread;
        ~JoinedWorker() {thread.join();}
    } worker{std::thread([&] {task();finished.store(true,std::memory_order_release);})};
    ++events->jobs;
    do {
        ScopedPreparationEvents suspend(nullptr);
        events->pump(events->user);
        ++events->pumps;
        // Do not enter a platform-runtime future/condition-variable wait while
        // owning the UI thread. An atomic completion latch keeps event service
        // independent of that wait implementation; get() is ready before use.
        if(!finished.load(std::memory_order_acquire))
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
    } while(!finished.load(std::memory_order_acquire));
    return result.get();
}

} // namespace starfox::render
