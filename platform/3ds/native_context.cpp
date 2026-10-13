#include "native_context.hpp"
#include "starfox/platform/nintendo_3ds/pica_context_owners.hpp"
#include <stdexcept>
#include <3ds.h>
#include <citro3d.h>
namespace starfox::platform::nintendo_3ds {
namespace { PicaContextOwners owners; }
bool acquire_native_context(void* owner, void (*suspend)(void*) noexcept) {
    // Native effects require the larger command buffer. Never reallocate it
    // when switching back to the reference presenter.
    return owners.acquire(owner, suspend, [] { return C3D_Init(1024U * 1024U); });
}
bool activate_native_context(void* owner) {
    const auto result = owners.activate(owner);
    if (result == PicaContextOwners::Activation::unowned)
        throw std::logic_error("3DS presentation context has no owner");
    return result == PicaContextOwners::Activation::changed;
}
void suspend_native_context(void* owner) noexcept { owners.suspend(owner); }
void release_native_context(void* owner) noexcept {
    owners.release(owner, [] {
        C3D_Fini();
        gfxSet3D(false);
        gfxSetScreenFormat(GFX_TOP, GSP_BGR8_OES);
        gfxSetScreenFormat(GFX_BOTTOM, GSP_BGR8_OES);
    });
}
}
