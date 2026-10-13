#pragma once
namespace starfox::platform::nintendo_3ds {
bool acquire_native_context(void*, void (*suspend)(void*) noexcept);
// True on a handoff; caller must restore formats/outputs/shader and all draw
// state before submission. Throws if the owner never acquired the context.
bool activate_native_context(void*);
void suspend_native_context(void*) noexcept;
void release_native_context(void*) noexcept;
}
