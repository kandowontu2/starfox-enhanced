#include "native_context.hpp"
#include <3ds.h>
#include <citro3d.h>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
using namespace starfox::platform::nintendo_3ds;
void require(bool value, const char* message) {if (!value) throw std::runtime_error(message);}
bool sdk_active{}, reject_init{};
unsigned inits{}, finishes{}, stereo_restores{}, format_restores{};
std::vector<unsigned> shutdown;
struct Presenter {
    bool queued{};
    unsigned joins{};
    static void join(void* pointer) noexcept {
        auto& self = *static_cast<Presenter*>(pointer);
        self.queued = false;++self.joins;
    }
};
}
extern "C" bool C3D_Init(std::size_t bytes) {
    require(!sdk_active && bytes == 1024U * 1024U, "Actual adapter duplicated SDK init or changed command-buffer budget");
    ++inits;
    if (reject_init) return false;
    sdk_active = true;return true;
}
extern "C" void C3D_Fini() {
    require(sdk_active, "Actual adapter retired a missing SDK context");
    sdk_active = false;++finishes;shutdown.push_back(1);
}
extern "C" void gfxSet3D(bool enabled) {
    require(!sdk_active && !enabled, "LCD restored before final GPU retirement");
    ++stereo_restores;shutdown.push_back(2);
}
extern "C" void gfxSetScreenFormat(gfxScreen_t screen, GSPGPU_FramebufferFormat format) {
    require(!sdk_active && format == GSP_BGR8_OES, "Final CPU menu format was not restored");
    ++format_restores;shutdown.push_back(screen == GFX_TOP ? 3 : 4);
}
int main() try {
    Presenter mono, effects;
    reject_init = true;
    require(!acquire_native_context(&mono, Presenter::join), "Failed actual SDK initialization acquired owner");
    release_native_context(&mono);require(finishes == 0 && format_restores == 0, "Failed init performed SDK/LCD shutdown");
    reject_init = false;
    require(acquire_native_context(&mono, Presenter::join), "Actual mono context lease failed");
    require(activate_native_context(&mono), "Actual first activation missing");mono.queued = true;
    require(acquire_native_context(&effects, Presenter::join), "Actual resident effects lease failed");
    require(inits == 2 && mono.queued, "Second actual owner reinitialized SDK or drained unnecessarily");
    bool unowned{};
    try {activate_native_context(nullptr);} catch (const std::logic_error&) {unowned = true;}
    require(unowned && mono.queued, "Unowned actual activation did not fail closed");
    for (unsigned i = 0; i < 2000; ++i) {
        auto& next = i % 2 ? mono : effects;auto& previous = i % 2 ? effects : mono;
        require(activate_native_context(&next) && !previous.queued, "Actual adapter did not join the previous owner's queue");
        next.queued = true;
        require(!activate_native_context(&next) && next.queued, "Actual repeated frame retired resident owner");
    }
    release_native_context(&effects);
    require(sdk_active && mono.queued && finishes == 0 && format_restores == 0, "Inactive close changed active SDK/LCD ownership");
    release_native_context(&effects);require(finishes == 0, "Double actual close retired SDK");
    suspend_native_context(&mono);require(!mono.queued && sdk_active, "Actual suspend destroyed shared SDK or missed join");
    require(activate_native_context(&mono), "Actual suspended owner did not reactivate");mono.queued = true;
    release_native_context(&mono);
    require(!mono.queued && !sdk_active && finishes == 1 && stereo_restores == 1 && format_restores == 2,
        "Actual final owner failed queue/SDK/LCD retirement");
    require(shutdown == std::vector<unsigned>{1,2,3,4}, "Actual final SDK/LCD retirement order changed");
    require(acquire_native_context(&effects, Presenter::join), "Actual SDK could not reopen after CPU menu");
    require(inits == 3 && activate_native_context(&effects), "Actual context restart lost owner");
    release_native_context(&effects);require(finishes == 2 && format_restores == 4, "Actual reopened context leaked");
    std::cout << "Actual native context adapter: 2000 switches, one SDK context, queue retirement and CPU-menu restoration passed\n";
} catch (const std::exception& error) {std::cerr << error.what() << '\n';return 1;}
