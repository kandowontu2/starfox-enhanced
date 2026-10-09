#include "starfox/render/face_material.hpp"
#include "starfox/render/software_renderer.hpp"
#include <iostream>
#include <stdexcept>
using namespace starfox;
namespace {void require(bool value) {if(!value) throw std::runtime_error("Face material assertion failed");}}
int main() try {
    assets::Shape shape;assets::Face face{};render::RenderPose pose;
    const std::array<std::int8_t,3> light{127,127,127};
    // This override must never index the intentionally empty color table.
    face.colour_id=255;
    auto material=render::face_material(shape,face,0,0,light,pose,0x006c);
    require(material.colour.even==12 && material.colour.odd==6 && material.colour.dither);
    material=render::face_material(shape,face,0,0,light,pose);
    require(material.colour.even==15 && !material.colour.dither);
    face.colour_id=0;shape.colour_words={0x006c};
    shape.colour_materials={{0x006c,{0x0011,0xc909,0x4000}}};
    assets::TextureImage texture;texture.descriptor=0x4000;shape.textures.push_back(texture);
    material=render::face_material(shape,face,4,0,light,pose);
    require(material.colour.even==9 && material.colour.odd==9 && !material.colour.dither);
    require(render::face_material(shape,face,5,0,light,pose).texture==&shape.textures[0]);
    pose.palette_override=203;
    material=render::face_material(shape,face,5,0,light,pose,std::nullopt,128);
    require(material.colour.even==75 && material.texture==nullptr);
    pose.palette_override.reset();pose.force_colour=true;pose.forced_colour=0x91;
    material=render::face_material(shape,face,5,0,light,pose);
    require(material.colour.even==1 && material.colour.odd==9 && material.colour.dither);
    pose.force_colour=false;shape.colour_materials.clear();shape.colour_words[0]=0x0200;
    shape.has_diffuse_shade_tables=true;face.normal={127,127,127};
    shape.diffuse_shade_tables[2][2][9]=0xa3;
    material=render::face_material(shape,face,0,2,light,pose);
    require(material.colour.even==3 && material.colour.odd==10);
    shape.colour_words[0]=0x3e05;pose.has_depth_colour_tables=true;pose.depth_colour_tables[3][5]=0xb4;
    material=render::face_material(shape,face,0,999,light,pose);
    require(material.colour.even==4 && material.colour.odd==11);
    std::cout<<"Shared source face-material tests passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
