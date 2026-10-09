#pragma once
#include "starfox/vr/eye_camera.hpp"
#include "starfox/render/shadow_mask.hpp"

namespace starfox::vr {
class ShadowView {
    using Vec3=render::shadows::Vec3;
    Matrix4 view_{};
    double units_{};
    ShadowView()=default;
public:
    static std::optional<ShadowView> from_eye(const EyeCamera& eye,double units=256) {
        if(!std::isfinite(units) || units<=0) return {};
        const auto& m=eye.view;
        for(auto value:m) if(!std::isfinite(value)) return {};
        if(m[3]!=0 || m[7]!=0 || m[11]!=0 || m[15]!=1) return {};
        // Eye transforms must be rigid: normals and lights use rotation only.
        for(unsigned a=0;a<3;++a) for(unsigned b=0;b<3;++b) {
            double product=0;for(unsigned k=0;k<3;++k) product+=double(m[k*4+a])*m[k*4+b];
            if(std::abs(product-(a==b?1.:0.))>1e-4) return {};
        }
        ShadowView result;result.view_=m;result.units_=units;return result;
    }
    Vec3 direction(Vec3 p) const {
        const auto& m=view_;
        return {m[0]*p.x+m[4]*p.y+m[8]*p.z,
            -(m[1]*p.x+m[5]*p.y+m[9]*p.z),-(m[2]*p.x+m[6]*p.y+m[10]*p.z)};
    }
    Vec3 point(Vec3 p) const {
        return (direction(p)+Vec3{view_[12],-view_[13],-view_[14]})*units_;
    }
    render::shadows::Triangle triangle(const render::shadows::Triangle& t) const {
        return {point(t.a),point(t.b),point(t.c)};
    }
    render::shadows::ReceiverPlane receiver(const render::shadows::ReceiverPlane& plane) const {
        return {point(plane.point),direction(plane.normal)};
    }
};
// Compose the same native-point -> XR-world -> eye mapping as scene.hlsl.
// Input native points are +Y down/+Z forward and divided by model_units in
// graphics; output is eye-local native shadow coordinates in shadow_units.
// Model placement can include nonuniform scale, but the tracked eye is rigid.
inline std::optional<std::array<std::array<float,4>,3>> shadow_model_transform(
    const EyeCamera& eye,const Matrix4& placement,double model_units,double shadow_units=256) {
    if(!std::isfinite(model_units) || model_units<=0 || !ShadowView::from_eye(eye,shadow_units)) return {};
    const auto composed=model_eye_camera(eye,placement);
    if(!composed) return {};
    const auto& m=composed->view;
    constexpr double sign[]{1,-1,-1};
    std::array<std::array<float,4>,3> rows{};
    for(unsigned row=0;row<3;++row) {
        for(unsigned column=0;column<3;++column)
            rows[row][column]=float(sign[row]*m[column*4+row]*sign[column]*shadow_units/model_units);
        rows[row][3]=float(sign[row]*m[12+row]*shadow_units);
        for(float value:rows[row]) if(!std::isfinite(value)) return {};
    }
    return rows;
}
// Native shadow rays use +Y down, +Z forward; XR eye space is +Y up,
// -Z forward. Geometry must be converted to that basis before tracing.
inline std::optional<render::shadows::Camera> shadow_camera(const EyeCamera& eye,
    uint32_t width,uint32_t height) {
    if(!width || !height || width>16384 || height>16384) return {};
    const auto& p=eye.projection;
    for(auto value:p) if(!std::isfinite(value)) return {};
    // Standard asymmetric perspective, not orthographic or a skewed frustum.
    if(p[0]<=0 || p[5]>=0 || p[11]!=-1 || p[15]!=0
        || p[1]!=0 || p[4]!=0 || p[3]!=0 || p[7]!=0 || p[12]!=0 || p[13]!=0) return {};
    return render::shadows::Camera{width,height,double(p[0])*width*.5,
        (1.-p[8])*width*.5,(1.-p[9])*height*.5,-double(p[5])*height*.5};
}
}
