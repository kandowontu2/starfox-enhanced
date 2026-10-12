#include "native_stem_executor.hpp"
#include <3ds.h>
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
#if defined(STARFOX_STEM_TEST_NEW_MODEL)
bool new_model{true};
#else
bool new_model{};
#endif
bool model_failure{}, create_failure{}, priority_failure{};
int configured_priority = 0x30, created_priority{}, created_core{};
std::size_t created_stack{};
unsigned creations{}, joins{}, frees{}, checks{};
void require(bool ok, const char* message) {
    ++checks;
    if (!ok) throw std::runtime_error(message);
}
using Executor = starfox::platform::nintendo_3ds::NativeStemExecutor;
using Task = starfox::audio::StemTask;
struct Work {
    unsigned calls{};
    std::thread::id owner;
    std::vector<unsigned> output;
    bool fail{};
    static void run(void* context) {
        auto& self = *static_cast<Work*>(context);
        self.owner = std::this_thread::get_id();
        ++self.calls;
        self.output.assign(3200, self.calls);
        if (self.fail) throw std::runtime_error("stem failure");
    }
    Task task() { return {this, run}; }
};
void serial_cases() {
    for (unsigned mode = 0; mode < 3; ++mode) {
        new_model = mode != 0; model_failure = mode == 1; create_failure = mode == 2;
        const auto created_before = creations, joined_before = joins;
        {
            Executor executor;
            require(!executor.parallel_available(), "Unsupported worker did not keep serial fallback");
            Work a, b;
            executor.execute(a.task(), b.task());
            require(a.calls == 1 && b.calls == 1, "Serial fallback lost a complete stem");
            require(a.owner == std::this_thread::get_id() && b.owner == a.owner,
                "Serial fallback unexpectedly started a worker");
            a.fail = true;
            bool caught{};
            try { executor.execute(a.task(), b.task()); }
            catch (const std::runtime_error&) { caught = true; }
            require(caught && a.calls == 2 && b.calls == 1, "Serial failure semantics changed");
            a.fail = false;
            executor.execute(a.task(), b.task());
            require(a.calls == 3 && b.calls == 2, "Failure left the executor permanently busy");
        }
        require(joins == joined_before && frees == joins, "Serial path freed/joined a nonexistent worker");
        require(creations == created_before + (mode == 2 ? 1U : 0U),
            "Old/failed model check attempted a New-model core allocation");
    }
    model_failure = create_failure = false;
}
void parallel_cases() {
    new_model = true;
    for (int priority : {0x05, 0x18, 0x30, 0x3f, 0x70}) {
        configured_priority = priority;
        const auto joined_before = joins;
        {
            Executor executor;
            require(executor.parallel_available(), "New worker was not created");
            require(created_core == 2 && created_stack == 512U * 1024U,
                "Worker stole system core or lost its full SPC upload stack");
            require(created_priority == (priority < 0x18 ? 0x18 : priority > 0x3f ? 0x3f : priority),
                "Worker priority exceeds documented user range");
            Work a, b;
            std::thread::id persistent;
            for (unsigned block = 1; block <= 1024; ++block) {
                executor.execute(a.task(), b.task());
                if (block == 1) persistent = b.owner;
                require(a.calls == block && b.calls == block, "A stem was duplicated/lost");
                require(a.owner == std::this_thread::get_id() && b.owner != a.owner && b.owner == persistent,
                    "Worker did not remain persistent or foreground moved threads");
                require(a.output.size() == 3200 && b.output.size() == 3200
                    && a.output.front() == block && b.output.back() == block,
                    "execute returned before the entire borrowed output was published");
            }
            for (unsigned fault = 1; fault <= 3; ++fault) {
                a.fail = (fault & 1U) != 0; b.fail = (fault & 2U) != 0;
                const auto previous = a.calls;
                bool caught{};
                try { executor.execute(a.task(), b.task()); }
                catch (const std::runtime_error&) { caught = true; }
                require(caught && a.calls == previous + 1 && b.calls == a.calls,
                    "Exception escaped before the started worker completed");
                a.fail = b.fail = false;
                executor.execute(a.task(), b.task());
                require(b.calls == a.calls, "Completed failure poisoned the next block");
            }
            const auto previous = a.calls;
            bool rejected{};
            try { executor.execute(a.task(), {}); }
            catch (const std::invalid_argument&) { rejected = true; }
            require(rejected && a.calls == previous, "Missing task was not rejected before launch");
        }
        require(joins == joined_before + 1 && frees == joins,
            "Executor retired storage before joining/freeing its actual worker");
    }
    priority_failure = true;
    { Executor executor; require(created_priority == 0x30, "Failed priority query lost safe default"); }
    priority_failure = false;
}
struct Overlap {
    Executor* executor{};
    Work* work{};
    bool rejected{};
    static void run(void* context) {
        auto& self = *static_cast<Overlap*>(context);
        try { self.executor->execute(self.work->task(), self.work->task()); }
        catch (const std::logic_error&) { self.rejected = true; }
    }
};
void overlap_case() {
    Executor executor;
    Work work;
    Overlap nested{&executor, &work};
    executor.execute({&nested, Overlap::run}, work.task());
    require(nested.rejected && work.calls == 1, "Reentrant foreground call overwrote a borrowed job");
    nested.rejected = false;
    executor.execute(work.task(), {&nested, Overlap::run});
    require(nested.rejected && work.calls == 2, "Reentrant worker call deadlocked or overwrote the job");
}
}
Result APT_CheckNew3DS(bool* model) { *model = new_model; return model_failure ? -1 : 0; }
Result svcGetThreadPriority(s32* priority, Handle handle) {
    require(handle == CUR_THREAD_HANDLE, "Queried an unrelated thread priority");
    *priority = configured_priority; return priority_failure ? -1 : 0;
}
Thread threadCreate(ThreadFunc entry, void* context, std::size_t stack, int priority, int core, bool detached) {
    ++creations; created_stack = stack; created_priority = priority; created_core = core;
    require(!detached, "Worker was detached, so owner cannot prove retirement");
    if (create_failure) return nullptr;
    return new Thread_tag{std::thread(entry, context)};
}
Result threadJoin(Thread thread, u64 timeout) {
    require(timeout == UINT64_MAX, "Worker join has a storage-unsafe timeout");
    thread->value.join(); ++joins; return 0;
}
void threadFree(Thread thread) {
    require(!thread->value.joinable(), "Freed a live worker"); ++frees; delete thread;
}
void LightLock_Init(LightLock*) {}
void LightLock_Lock(LightLock* lock) { lock->value.lock(); }
void LightLock_Unlock(LightLock* lock) { lock->value.unlock(); }
void LightEvent_Init(LightEvent* event, ResetType reset) { event->reset = reset; }
void LightEvent_Signal(LightEvent* event) {
    { std::lock_guard guard(event->lock); event->ready = true; }
    event->signal.notify_one();
}
void LightEvent_Wait(LightEvent* event) {
    std::unique_lock guard(event->lock);
    event->signal.wait(guard, [&] { return event->ready; });
    if (event->reset == RESET_ONESHOT) event->ready = false;
}
int main() try {
    serial_cases(); parallel_cases(); overlap_case();
    require(joins == frees, "A worker outlived its source task owner");
    std::cout << "Native stem executor: " << checks << " host synchronization/retirement checks passed; "
        << "not accurate SPC PCM, ARM, core permissions or console performance acceptance.\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
