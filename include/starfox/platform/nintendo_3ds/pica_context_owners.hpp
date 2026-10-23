#pragma once
#include <array>
#include <cstddef>

namespace starfox::platform::nintendo_3ds {
// Main-thread-only ownership of ONE SDK context and at most two resident
// presenters. Switching must drain/detach the old presenter's outputs, not
// destroy its textures, targets, shader or CPU planning caches. Call outside
// an acquired frame. Resource deletion remains the presenter's responsibility.
class PicaContextOwners {
public:
    using Suspend = void (*)(void*) noexcept;
    enum class Activation { unowned, unchanged, changed };
    template<class Initialize>
    bool acquire(void* owner, Suspend suspend, Initialize initialize) {
        if (!owner || !suspend || find(owner)) return false;
        Slot* vacant = nullptr;
        for (auto& slot : slots_) if (!slot.owner) { vacant = &slot; break; }
        if (!vacant || (empty() && !initialize())) return false;
        *vacant = {owner, suspend};
        return true;
    }
    Activation activate(void* owner) noexcept {
        auto* next = find(owner);
        if (!next) return Activation::unowned;
        if (active_ == next) return Activation::unchanged;
        if (active_) active_->suspend(active_->owner);
        active_ = next;
        return Activation::changed;
    }
    void suspend(void* owner) noexcept {
        if (active_ && active_->owner == owner) {
            active_->suspend(owner);
            active_ = nullptr;
        }
    }
    template<class Finish>
    void release(void* owner, Finish finish) noexcept {
        auto* slot = find(owner);
        if (!slot) return;
        suspend(owner);
        *slot = {};
        if (empty()) finish();
    }
    bool empty() const noexcept {
        for (const auto& slot : slots_) if (slot.owner) return false;
        return true;
    }
private:
    struct Slot { void* owner{}; Suspend suspend{}; };
    Slot* find(void* owner) noexcept {
        if (owner) for (auto& slot : slots_) if (slot.owner == owner) return &slot;
        return nullptr;
    }
    std::array<Slot, 2> slots_{};
    Slot* active_{};
};
}
