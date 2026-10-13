#include "native_stem_executor.hpp"
#include "starfox/platform/nintendo_3ds/startup_trace.hpp"
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
    bool stopping{}, busy{}, deferred{};
    u32 previous_budget{};
    bool budget_changed{};

    explicit Impl(Role role) {
        LightLock_Init(&lock);
        LightEvent_Init(&ready, RESET_ONESHOT);
        LightEvent_Init(&done, RESET_ONESHOT);
        bool new_model{};
        if (R_FAILED(APT_CheckNew3DS(&new_model)) || !new_model) return;
        const bool effects=role==Role::budgeted_effects;
        if(effects) {
            // Core 1 is shared with system services, not an unrestricted extra
            // application core. Refuse it when APT cannot grant a budget.
            if(R_FAILED(APT_GetAppCpuTimeLimit(&previous_budget))) return;
            if(previous_budget<50) {
                if(R_FAILED(APT_SetAppCpuTimeLimit(70))
                    && R_FAILED(APT_SetAppCpuTimeLimit(50))) return;
                budget_changed=true;
            }
        }
        s32 priority = 0x30;
        if (R_FAILED(svcGetThreadPriority(&priority, CUR_THREAD_HANDLE))) priority = 0x30;
        priority = std::clamp<s32>(priority, 0x18, 0x3f);
        // Core 2 is permission-dependent even on New 3DS. threadCreate returns
        // nullptr on failure, which leaves the serial implementation intact.
        worker = threadCreate(entry, this, worker_stack_bytes, priority, effects?1:2, false);
        if(!worker) restore_budget();
    }
    void restore_budget() noexcept {
        if(budget_changed) {
            static_cast<void>(APT_SetAppCpuTimeLimit(previous_budget));
            budget_changed=false;
        }
    }
    ~Impl() {
        if (!worker) return;
        // A dispatch may not have started yet. Finish it before setting stop;
        // otherwise the worker could consume ready, see stop and drop the job.
        try {finish();} catch(...) {}
        LightLock_Lock(&lock);
        stopping = true;
        LightLock_Unlock(&lock);
        LightEvent_Signal(&ready);
        // No deadline/free-while-live fallback: the worker owns borrowed state.
        if (R_FAILED(threadJoin(worker, std::numeric_limits<u64>::max())))
            std::terminate();
        threadFree(worker);
        restore_budget();
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
    bool begin(audio::StemTask background) {
        if(!background.run) throw std::invalid_argument("Missing background SPC task");
        LightLock_Lock(&lock);
        if(busy) {
            LightLock_Unlock(&lock);
            throw std::logic_error("Overlapping SPC stem executor call");
        }
        if(!worker) {LightLock_Unlock(&lock);return false;}
        busy=true;deferred=true;task=background;failure={};
        LightLock_Unlock(&lock);
        STARFOX_3DS_STARTUP_DETAIL_MARK(audio_worker_dispatch);
        LightEvent_Signal(&ready);
        return true;
    }
    void finish() {
        LightLock_Lock(&lock);
        const bool pending=busy;
        if(pending && !deferred) {
            LightLock_Unlock(&lock);
            throw std::logic_error("Cannot finish a synchronous SPC executor call");
        }
        LightLock_Unlock(&lock);
        if(!pending) return;
        STARFOX_3DS_STARTUP_DETAIL_MARK(audio_worker_wait);
        LightEvent_Wait(&done);
        STARFOX_3DS_STARTUP_DETAIL_MARK(audio_worker_complete);
        LightLock_Lock(&lock);
        const auto error=failure;failure={};busy=false;deferred=false;
        LightLock_Unlock(&lock);
        if(error) std::rethrow_exception(error);
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
    STARFOX_3DS_STARTUP_DETAIL_MARK(audio_worker_dispatch);
            LightEvent_Signal(&ready);
            try { foreground.run(foreground.context); }
            catch (...) { foreground_failure = std::current_exception(); }
            // Always join the in-flight block, including when foreground fails.
    STARFOX_3DS_STARTUP_DETAIL_MARK(audio_worker_wait);
            LightEvent_Wait(&done);
    STARFOX_3DS_STARTUP_DETAIL_MARK(audio_worker_complete);
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
NativeStemExecutor::NativeStemExecutor(Role role) : impl_(std::make_unique<Impl>(role)) {}
NativeStemExecutor::~NativeStemExecutor() = default;
bool NativeStemExecutor::parallel_available() const noexcept { return impl_->worker != nullptr; }
void NativeStemExecutor::execute(audio::StemTask foreground, audio::StemTask background) {
    impl_->execute(foreground, background);
}
bool NativeStemExecutor::begin(audio::StemTask background) {return impl_->begin(background);}
void NativeStemExecutor::finish() {impl_->finish();}
} // namespace starfox::platform::nintendo_3ds
