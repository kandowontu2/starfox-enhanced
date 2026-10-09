#pragma once
#include "starfox/vr/eye_camera.hpp"
#include <cmath>
#include <optional>
#include <utility>

namespace starfox::render {
// Native current-eye position -> the same rigid surface's previous-eye
// position. Matrices only: geometry stays resident and transforms on the GPU.
// Correspondence/accepted-history ownership belongs to the caller; a matrix
// alone does not prove that an animated or recycled object is the same surface.
inline std::optional<vr::Matrix4> calibrated_motion_mapping(
    const vr::EyeCamera& current,const vr::EyeCamera& previous,
    const vr::Matrix4& model,const vr::Matrix4& previous_model) noexcept {
    const auto now=vr::model_eye_camera(current,model);
    const auto then=vr::model_eye_camera(previous,previous_model);
    if(!now || !then) return {};
    const auto canonical=[](const vr::Matrix4& p) {
        for(float value:p) if(!std::isfinite(value)) return false;
        return p[0]>0 && p[5]>0 && p[1]==0 && p[2]==0 && p[3]==0 && p[4]==0
            && p[6]==0 && p[7]==0 && p[11]==-1 && p[12]==0 && p[13]==0
            && p[15]==0 && p[10]<=-1 && p[14]<0;
    };
    if(!canonical(now->projection) || !canonical(then->projection)) return {};
    for(const auto* view:{&now->view,&then->view})
        if((*view)[3]!=0 || (*view)[7]!=0 || (*view)[11]!=0 || (*view)[15]!=1) return {};
    const auto& old=then->view;
    const double determinant=double(old[0])*(double(old[5])*old[10]-double(old[9])*old[6])
        -double(old[4])*(double(old[1])*old[10]-double(old[9])*old[2])
        +double(old[8])*(double(old[1])*old[6]-double(old[5])*old[2]);
    if(!std::isfinite(determinant) || std::abs(determinant)<1.e-20) return {};
    double rows[4][8]{};
    for(unsigned r=0;r<4;++r) for(unsigned c=0;c<4;++c) {
        const double value=now->view[c*4+r];
        if(!std::isfinite(value) || std::abs(value)>1.e8) return {};
        rows[r][c]=value;rows[r][c+4]=r==c;
    }
    for(unsigned c=0;c<4;++c) {
        unsigned pivot=c;
        for(unsigned r=c+1;r<4;++r) if(std::abs(rows[r][c])>std::abs(rows[pivot][c])) pivot=r;
        if(std::abs(rows[pivot][c])<1.e-12) return {};
        for(unsigned k=0;k<8;++k) std::swap(rows[pivot][k],rows[c][k]);
        const double divisor=rows[c][c];for(double& value:rows[c]) value/=divisor;
        for(unsigned r=0;r<4;++r) if(r!=c) {
            const double factor=rows[r][c];
            for(unsigned k=0;k<8;++k) rows[r][k]-=factor*rows[c][k];
        }
    }
    vr::Matrix4 result{};
    for(unsigned r=0;r<4;++r) for(unsigned c=0;c<4;++c) {
        double value=0;
        for(unsigned k=0;k<4;++k) value+=double(then->view[k*4+r])*rows[k][c+4];
        if(!std::isfinite(value) || std::abs(value)>1.e8) return {};
        result[c*4+r]=float(value);
    }
    return result;
}
}
