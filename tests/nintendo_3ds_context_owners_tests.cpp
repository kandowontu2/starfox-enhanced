#include "starfox/platform/nintendo_3ds/pica_context_owners.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using Owners = starfox::platform::nintendo_3ds::PicaContextOwners;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Presenter {
    bool pending{}, linked{}, visible{}, resources{true};
    unsigned suspends{}, cache_generation{1};
    static void suspend(void* pointer) noexcept {
        auto& self = *static_cast<Presenter*>(pointer);
        // SDK output detachment joins the command queue. Model the same order.
        self.pending = false;self.linked = false;self.visible = false;++self.suspends;
    }
    void submit() { require(resources && linked, "Submission without live resources/output");pending = visible = true; }
};
}
int main() try {
    Owners owners;
    Presenter mono, effects, extra;
    unsigned starts{}, stops{};
    const auto start = [&] { ++starts;return true; };
    const auto stop = [&]() noexcept { ++stops; };
    require(owners.empty(), "Cold context is not empty");
    require(!owners.acquire(nullptr, Presenter::suspend, start), "Null owner accepted");
    require(!owners.acquire(&mono, nullptr, start), "Missing queue-retirement callback accepted");
    require(!owners.acquire(&mono, Presenter::suspend, [] { return false; }) && owners.empty(), "Failed SDK init retained owner");
    require(owners.activate(&mono) == Owners::Activation::unowned, "Unacquired presenter activated");
    owners.release(&mono, stop);require(stops == 0, "Cold/double close retired SDK");
    require(owners.acquire(&mono, Presenter::suspend, start), "Mono lease failed");
    require(!owners.acquire(&mono, Presenter::suspend, start), "Duplicate owner accepted");
    require(owners.activate(&mono) == Owners::Activation::changed, "First activation missing");
    mono.linked = true;mono.submit();
    require(owners.acquire(&effects, Presenter::suspend, start), "Second resident lease failed");
    require(starts == 1 && mono.pending, "Acquisition rebuilt SDK or modified active outputs");
    require(!owners.acquire(&extra, Presenter::suspend, start), "Unbounded presenter storage accepted");
    require(owners.activate(&extra) == Owners::Activation::unowned && mono.pending, "Unknown activation disturbed live frame");
    for (unsigned frame = 0; frame < 2000; ++frame) {
        auto& next = frame % 2 ? mono : effects;
        auto& previous = frame % 2 ? effects : mono;
        require(owners.activate(&next) == Owners::Activation::changed, "Handoff did not activate resident owner");
        require(!previous.pending && !previous.linked && !previous.visible, "New owner acquired before old GPU queue drained");
        require(next.resources && previous.resources && next.cache_generation == 1 && previous.cache_generation == 1,
            "Handoff destroyed resources or CPU caches");
        next.linked = true;next.submit();
        const auto suspends = next.suspends;
        require(owners.activate(&next) == Owners::Activation::unchanged && next.suspends == suspends && next.pending,
            "Repeated frame added a handoff/queue wait");
        require(starts == 1 && stops == 0, "Handoff reinitialized or destroyed SDK context");
    }
    // Partial/failed construction can release an inactive lease without
    // destroying the active presenter's SDK or borrowing its outputs.
    owners.release(&effects, stop);effects.resources = false;
    require(mono.pending && stops == 0 && !owners.empty(), "Inactive close killed the active context");
    owners.release(&effects, stop);require(stops == 0, "Repeated inactive close retired SDK");
    require(owners.acquire(&effects, Presenter::suspend, start) && starts == 1, "Reopen rebuilt shared SDK");
    effects.resources = true;
    require(owners.activate(&effects) == Owners::Activation::changed && !mono.pending, "Reopened owner missed queue join");
    effects.linked = true;effects.submit();
    owners.suspend(&effects);
    require(!effects.pending && !effects.linked && !effects.visible, "Explicit suspension left GPU use pending");
    const auto suspended = effects.suspends;
    owners.suspend(&effects);require(effects.suspends == suspended, "Double suspension repeated GPU work");
    owners.release(&mono, stop);owners.release(&effects, stop);
    require(owners.empty() && stops == 1, "Final owner did not retire SDK exactly once");
    require(owners.acquire(&mono, Presenter::suspend, start) && starts == 2, "Context could not reopen after plain menu");
    owners.release(&mono, stop);require(stops == 2, "Reopened final lease leaked SDK");
    std::cout << "Shared PICA context: 2000 resident handoffs, queue drain, failures, bounded leases and final retirement passed\n";
} catch (const std::exception& error) { std::cerr << error.what() << '\n';return 1; }
