#pragma once
#include "starfox/render/fsr1_settings.hpp"
#include <memory>
#include <string>
namespace starfox::render {
// Spatial presentation fallback for SDL's D3D11/WinRT backend. Does not
// pretend to reduce the game's CPU raster work or implement temporal AA.
class D3d11Fsr1 {
public:
    D3d11Fsr1();~D3d11Fsr1();
    bool apply(void* device,void* source,void* destination,Fsr1Mode mode);
    void reset();
    const std::string& status() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
