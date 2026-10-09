#pragma once
#include "starfox/vr/eye_camera.hpp"
#include <cmath>
namespace starfox::render {
struct CalibratedEnvironmentCameras {
    std::array<vr::EyeCamera,6> faces;
    // DXR eye (+Y down/+Z forward) -> world cube in that same convention.
    std::array<float,9> ray_to_cube;
};
// Matrix metadata only: native source vertices/pixels never visit the CPU.
// Faces agree with the DXR cube's +X,-X,+Y,-Y,+Z,-Z and edge-tap convention.
inline std::optional<CalibratedEnvironmentCameras> calibrated_environment_cameras(
    const vr::EyeCamera& eye,float near_plane=1.F/256) noexcept {
    for(float value:eye.view) if(!std::isfinite(value)) return std::nullopt;
    if(eye.view[3]!=0 || eye.view[7]!=0 || eye.view[11]!=0 || eye.view[15]!=1
        || !std::isfinite(near_plane) || near_plane<=0) return std::nullopt;
    for(unsigned r=0;r<3;++r) for(unsigned s=0;s<3;++s) {
        float dot=0;for(unsigned c=0;c<3;++c) dot+=eye.view[c*4+r]*eye.view[c*4+s];
        if(std::abs(dot-float(r==s))>.0001F) return std::nullopt;
    }
    constexpr float sign[3]={1,-1,-1};
    constexpr float basis[6][9]={
        {0,0,-1, 0,1,0, -1,0,0},{0,0,1, 0,1,0, 1,0,0},
        {1,0,0, 0,0,-1, 0,-1,0},{1,0,0, 0,0,1, 0,1,0},
        {1,0,0, 0,1,0, 0,0,-1},{-1,0,0, 0,1,0, 0,0,1}};
    float position[3]{};
    CalibratedEnvironmentCameras result;
    for(unsigned r=0;r<3;++r) {
        for(unsigned c=0;c<3;++c) {
            position[r]-=eye.view[r*4+c]*eye.view[12+c];
            result.ray_to_cube[r*3+c]=sign[r]*eye.view[r*4+c]*sign[c];
        }
    }
    for(unsigned f=0;f<6;++f) {
        auto& camera=result.faces[f];camera.view={};camera.view[15]=1;
        camera.effects=eye.effects;
        for(unsigned r=0;r<3;++r) for(unsigned c=0;c<3;++c) {
            camera.view[c*4+r]=basis[f][r*3+c]*sign[c];
            camera.view[12+r]-=camera.view[c*4+r]*position[c];
        }
        camera.projection={1,0,0,0, 0,1,0,0, 0,0,-1,-1, 0,0,-near_plane,0};
    }
    return result;
}
}
