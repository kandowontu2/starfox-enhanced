#pragma once
#include "starfox/platform/nintendo_3ds/game_session.hpp"
#include <memory>
namespace starfox::platform::nintendo_3ds {
// Reference mono presentation owns Citro2D/Citro3D exclusively. The caller
// must retire NativeGpu before constructing it, and retire this before stereo.
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
