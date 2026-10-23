#pragma once
#include "starfox/platform/nintendo_3ds/game_session.hpp"
#include <memory>
namespace starfox::platform::nintendo_3ds {
// Reference mono retains its art/planning resources beside NativeGpu. Only
// the activated owner submits; retire both before NativeDisplay CPU swaps.
class NativeMono {
public:
    NativeMono();~NativeMono();
    NativeMono(const NativeMono&)=delete;
    NativeMono& operator=(const NativeMono&)=delete;
    bool present(GameSession&,const GamePresentation&,ImageView dashboard);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
