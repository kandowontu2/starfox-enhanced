#pragma once
#include <cstdint>
namespace starfox::render {
// Internal, opt-in encoding diagnostics. The scene owns the query/fence; the
// model only marks boundaries on that same command. Parallel eyes use separate
// thread-local hooks, and standalone/native callers keep the null default.
struct ModelGpuTimingHook {
    void* owner{};int index{-1};
    void (*mark)(void*,int,void*) noexcept{};
    std::uint64_t batch{};
    unsigned draw_index{};
};
inline thread_local ModelGpuTimingHook model_gpu_timing_hook;
struct ScopedModelGpuTimingHook {
    ModelGpuTimingHook previous;
    explicit ScopedModelGpuTimingHook(ModelGpuTimingHook next):previous(model_gpu_timing_hook) {
        model_gpu_timing_hook=next;
    }
    ~ScopedModelGpuTimingHook(){model_gpu_timing_hook=previous;}
};
inline void mark_model_gpu_time(void* command) noexcept {
    const auto hook=model_gpu_timing_hook;
    if(hook.mark) hook.mark(hook.owner,hook.index,command);
}
}
