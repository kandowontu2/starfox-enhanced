#pragma once
#include "starfox/render/calibrated_ground.hpp"
#include "starfox/render/dxr_shadows.hpp"
#include "starfox/vr/eye_camera.hpp"

namespace starfox::render {
struct CalibratedGroundRayReceiver {
    shadows::ReceiverPlane plane;
    shadows::RayWater transport;
};
// Only retained metadata/matrices are converted here. No vertex projection,
// per-eye pixels or CPU reflection images. The GPU uses the exact source floor
// plane and the same full calibrated eye/model transform as primary raster.
inline std::optional<CalibratedGroundRayReceiver> calibrated_ground_ray_receiver(
    const vr::DrawPacket& packet,const vr::EyeCamera& camera,float strength,bool mirror_models=false,
    const vr::Matrix4* model_override=nullptr,unsigned caustics=0) {
    const auto material=calibrated_ground_material(packet);
    if(!calibrated_ground_ray_surface(material)) return std::nullopt;
    if(!std::isfinite(strength) || strength<0 || strength>1)
        throw std::invalid_argument("Invalid native ground reflection strength");
    if(caustics>3) throw std::invalid_argument("Invalid native water caustics quality");
    const auto view=vr::model_eye_camera(camera,model_override?*model_override:packet.model);
    if(!view) throw std::invalid_argument("Invalid native ground model/eye");
    const auto& m=view->view;const auto words=packet.geometry.texel_view();
    const auto offset=std::size_t(packet.geometry.vertex_view().front().texture[0])+calibrated_ground_offset;
    const float height=std::bit_cast<float>(words[offset-1]);
    if(!std::isfinite(height) || height>=0 || height< -8)
        throw std::invalid_argument("Invalid native reflective ground height");
    // Source/ray space uses +Y down/+Z forward; scene space uses +Y up/-Z.
    constexpr float sign[]{1,-1,-1};
    CalibratedGroundRayReceiver result;
    result.plane.point={256*(m[4]*height+m[12]),-256*(m[5]*height+m[13]),-256*(m[6]*height+m[14])};
    const shadows::Vec3 x{m[0],-m[1],-m[2]},z{-m[8],m[9],m[10]};
    auto normal=shadows::cross(x,z);const auto squared=shadows::dot(normal,normal);
    if(!std::isfinite(squared) || squared<1.e-20)
        throw std::invalid_argument("Degenerate native reflective ground plane");
    result.plane.normal=normal*(1/std::sqrt(squared));
    auto& surface=result.transport;surface.material=material==9?3:material-5;surface.reflection_strength=strength;
    surface.mirror_models=mirror_models;surface.time=std::bit_cast<float>(words[offset+11]);
    surface.brightness=std::bit_cast<float>(words[offset+12]);
    if(material==5) {
        surface.caustics=caustics;
        surface.source_colour.emplace();
        for(unsigned c=0;c<3;++c) (*surface.source_colour)[c]=
            (std::bit_cast<float>(words[offset+c])+std::bit_cast<float>(words[offset+3+c]))*.5F;
    }
    float linear[9];
    for(unsigned row=0;row<3;++row) for(unsigned col=0;col<3;++col)
        linear[row*3+col]=m[col*4+row]*sign[row]*sign[col];
    // Inverse transpose serves both the row-vector view->world positions and
    // world->view normals. A transpose alone quietly assumes an orthonormal
    // source Q15 matrix; cartridge rotations are not exactly unit length.
    const float cofactor[]{linear[4]*linear[8]-linear[5]*linear[7],linear[5]*linear[6]-linear[3]*linear[8],linear[3]*linear[7]-linear[4]*linear[6],
        linear[2]*linear[7]-linear[1]*linear[8],linear[0]*linear[8]-linear[2]*linear[6],linear[1]*linear[6]-linear[0]*linear[7],
        linear[1]*linear[5]-linear[2]*linear[4],linear[2]*linear[3]-linear[0]*linear[5],linear[0]*linear[4]-linear[1]*linear[3]};
    const float determinant=linear[0]*cofactor[0]+linear[1]*cofactor[1]+linear[2]*cofactor[2];
    if(!std::isfinite(determinant) || std::abs(determinant)<1.e-12)
        throw std::invalid_argument("Degenerate native ground coordinate transform");
    for(unsigned i=0;i<9;++i) surface.world_to_view[i]=cofactor[i]/determinant;
    // Recover source-world coordinates from this exact calibrated eye/model;
    // both eyes and in-flight retries retain the same origin/wave clock.
    const float origin[]{std::bit_cast<float>(words[offset+9]),0,std::bit_cast<float>(words[offset+10])};
    for(unsigned axis=0;axis<3;++axis) {
        float value=origin[axis];
        for(unsigned row=0;row<3;++row) value-=surface.world_to_view[row*3+axis]*256*sign[row]*m[12+row];
        surface.camera_position[axis]=value;
    }
    return result;
}
}
