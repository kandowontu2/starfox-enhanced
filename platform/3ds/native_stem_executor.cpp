#include "native_stem_executor.hpp"
#include <3ds.h>
#include <algorithm>
#include <exception>
#include <limits>
#include <stdexcept>

namespace starfox::platform::nintendo_3ds {
struct NativeStemExecutor::Impl {
    // Upload replacement can use complete 64-KiB SPC snapshots on the stack.
    // Do not use libctru's small default stack for the accurate driver owner.
    static constexpr std::size_t worker_stack_bytes = 512U * 1024U;
    LightLock lock{};
    LightEvent ready{}, done{};
    Thread worker{};
    audio::StemTask task{};
    std::exception_ptr failure;
    bool stopping{}, busy{};

    Impl() {
        LightLock_Init(&lock);
        LightEvent_Init(&ready, RESET_ONESHOT);
        LightEvent_Init(&done, RESET_ONESHOT);
        bool new_model{};
        if (R_FAILED(APT_CheckNew3DS(&new_model)) || !new_model) return;
        s32 priority = 0x30;
        if (R_FAILED(svcGetThreadPriority(&priority, CUR_THREAD_HANDLE))) priority = 0x30;
        priority = std::clamp<s32>(priority, 0x18, 0x3f);
        // Core 2 is permission-dependent even on New 3DS. threadCreate returns
        // nullptr on failure, which leaves the serial implementation intact.
        worker = threadCreate(entry, this, worker_stack_bytes, priority, 2, false);
    }
    ~Impl() {
        if (!worker) return;
        LightLock_Lock(&lock);
        stopping = true;
        LightLock_Unlock(&lock);
        LightEvent_Signal(&ready);
        // No deadline/free-while-live fallback: the worker owns borrowed state.
        if (R_FAILED(threadJoin(worker, std::numeric_limits<u64>::max())))
            std::terminate();
        threadFree(worker);
    }
    static void entry(void* context) noexcept {
        auto& self = *static_cast<Impl*>(context);
        for (;;) {
            LightEvent_Wait(&self.ready);
            LightLock_Lock(&self.lock);
            const bool stop = self.stopping;
            const auto work = self.task;
            LightLock_Unlock(&self.lock);
            if (stop) return;
            std::exception_ptr error;
            try { work.run(work.context); }
            catch (...) { error = std::current_exception(); }
            LightLock_Lock(&self.lock);
            self.failure = error;
            self.task = {};
            LightLock_Unlock(&self.lock);
            LightEvent_Signal(&self.done);
        }
    }
    void execute(audio::StemTask foreground, audio::StemTask background) {
        if (!foreground.run || !background.run)
            throw std::invalid_argument("Missing synchronous SPC stem task");
        LightLock_Lock(&lock);
        if (busy) {
            LightLock_Unlock(&lock);
            throw std::logic_error("Overlapping SPC stem executor call");
        }
        busy = true;
        if (worker) { task = background; failure = {}; }
        LightLock_Unlock(&lock);

        std::exception_ptr foreground_failure, background_failure;
        if (worker) {
            LightEvent_Signal(&ready);
            try { foreground.run(foreground.context); }
            catch (...) { foreground_failure = std::current_exception(); }
            // Always join the in-flight block, including when foreground fails.
            LightEvent_Wait(&done);
            LightLock_Lock(&lock);
            background_failure = failure;
            failure = {};
            busy = false;
            LightLock_Unlock(&lock);
        } else {
            try { foreground.run(foreground.context); background.run(background.context); }
            catch (...) { foreground_failure = std::current_exception(); }
            LightLock_Lock(&lock);
            busy = false;
            LightLock_Unlock(&lock);
        }
        if (foreground_failure) std::rethrow_exception(foreground_failure);
        if (background_failure) std::rethrow_exception(background_failure);
    }
};
NativeStemExecutor::NativeStemExecutor() : impl_(std::make_unique<Impl>()) {}
NativeStemExecutor::~NativeStemExecutor() = default;
bool NativeStemExecutor::parallel_available() const noexcept { return impl_->worker != nullptr; }
void NativeStemExecutor::execute(audio::StemTask foreground, audio::StemTask background) {
    impl_->execute(foreground, background);
}
} // namespace starfox::platform::nintendo_3ds
