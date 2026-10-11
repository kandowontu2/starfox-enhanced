#pragma once
#include <memory>
#include <utility>

namespace starfox::platform::nintendo_3ds {
// Citro3D 1.7.1 retains its borrowed program pointer after C3D_Fini/Init.
// The previous CPU shader must survive until BindProgram has inspected it.
// Only one retired program is retained; no textures, targets or VBOs survive.
template<class Program>
class PicaProgramBinding {
public:
    template<class Bind>
    void bind(Program& next, Bind&& bind_program) {
        std::forward<Bind>(bind_program)(next);
        bound_ = &next;
        retired_.reset(); // The library no longer borrows the previous owner.
    }
    // Call only after C3D_Fini has drained commands/transfers. An allocation
    // which never reached BindProgram must not replace the still-borrowed one.
    void retire(std::unique_ptr<Program>& owner) noexcept {
        if(owner && owner.get() == bound_) retired_ = std::move(owner);
        else owner.reset();
    }
private:
    Program* bound_{};
    std::unique_ptr<Program> retired_;
};
}
