#include "starfox/render/calibrated_dlss_api.hpp"
#include "starfox/render/dlss_presentation_lifecycle.hpp"
#include <iostream>
#include <type_traits>

static_assert(std::is_standard_layout_v<starfox::render::CalibratedDlssApi>);
static_assert(std::is_trivially_copyable_v<starfox::render::CalibratedDlssApi>);

int main() {
    // Ordinary mobile/UWP/Linux stubs return this without XR headers or a SDK.
    const starfox::render::CalibratedDlssApi unavailable;
    if(unavailable.complete() || unavailable.module || unavailable.bind
        || unavailable.configure || unavailable.configure_model || unavailable.evaluate
        || unavailable.release || unavailable.finish_frame || unavailable.evaluate_rejection) return 1;
    starfox::render::DlssPresentationLifecycle lifecycle;
    unsigned calls=0;
    const auto finish=[&] {++calls;return true;};
    if(!lifecycle.finish(finish) || calls || lifecycle.pending()) return 2;
    for(unsigned i=0;i<128;++i) {
        lifecycle.touch();
        if(!lifecycle.pending() || !lifecycle.finish(finish) || lifecycle.pending()
            || !lifecycle.finish(finish) || calls!=i+1) return 3;
    }
    lifecycle.touch();
    if(lifecycle.finish([&] {++calls;return false;}) || !lifecycle.pending()
        || lifecycle.completed()!=128 || lifecycle.attempts()!=129) return 4;
    try {lifecycle.finish([]()->bool {throw 1;});return 5;} catch(int) {}
    if(!lifecycle.pending() || lifecycle.completed()!=128 || lifecycle.attempts()!=130) return 6;
    if(!lifecycle.finish(finish) || lifecycle.pending() || calls!=130
        || lifecycle.completed()!=129 || lifecycle.attempts()!=131) return 7;
    lifecycle={};
    if(lifecycle.pending() || lifecycle.completed() || lifecycle.attempts()) return 8;
    std::cout<<"PASS: no-XR DLSS ABI; once-only presentation completion, held/cancelled tickets, failed/throwing notifications and restart\n";
    return 0;
}
