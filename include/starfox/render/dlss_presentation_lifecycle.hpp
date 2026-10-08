#pragma once
#include <cstdint>

namespace starfox::render {
// SDK evaluation (including a cancelled command) and held-image presentation
// both need frame-end bookkeeping. Plain menus/black startup frames do not.
// A failed notification retains its ticket; successful completion is once-only.
class DlssPresentationLifecycle {
public:
    void touch() noexcept {pending_=true;}
    bool pending() const noexcept {return pending_;}
    std::uint64_t attempts() const noexcept {return attempts_;}
    std::uint64_t completed() const noexcept {return completed_;}
    template<class Finish> bool finish(Finish notify) {
        if(!pending_) return true;
        ++attempts_;
        if(!notify()) return false;
        pending_=false;++completed_;return true;
    }
private:
    bool pending_{};
    std::uint64_t attempts_{},completed_{};
};
}
