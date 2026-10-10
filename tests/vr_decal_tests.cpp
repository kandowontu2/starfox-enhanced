#include "starfox/assets/shape_decoder.hpp"
#include "starfox/vr/decal_surface.hpp"
#include "starfox/vr/draw_packet.hpp"
#include "starfox/vr/source_span_model.hpp"
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace starfox;
namespace {
void require(bool value,const std::source_location& where=std::source_location::current()) {
    if(!value) throw std::runtime_error("Decal assertion failed at line "+std::to_string(where.line()));
}
void check(const assets::Shape& shape,size_t face,bool expected,render::RenderPose pose={}) {
    render::Palette256 palette{};
    for(auto& colour:palette) colour={255,255,255,255};
    pose.z=950;pose.continuous_geometry=true;
    vr::DrawPacket packet;vr::SourceSpanModel resident;std::string error;
    require(vr::build_draw_packet(shape,pose,palette,0,1,false,256,packet,error));
    bool found=false;
    for(const auto& range:packet.geometry.ranges) {
        for(size_t i=range.first_vertex;i<range.first_vertex+range.vertex_count;++i) {
            const auto flags=packet.geometry.vertices[i].texture[3];
            if(range.source_face==face) {found=true;require(flags&1U);require(bool(flags&32768U)==expected);}
            else require(!(flags&32768U));
        }
    }
    require(found);
    for(bool unclipped:{false,true}) {
        require(vr::prepare_source_span_model(shape,pose,{},224,192,resident,error,unclipped));
        require(resident.faces.materials[face].textured);
        require(bool(resident.faces.polygons[face][3]&16U)==expected);
        for(size_t i=0;i<resident.faces.polygons.size();++i)
            if(i!=face) require(!(resident.faces.polygons[i][3]&16U));
    }
}
assets::Shape specimen() {
    assets::Shape shape;
    shape.vertices={{-30,-60,20},{30,-60,20},{30,60,20},{-30,60,20},
        {-10,-40,20},{10,-40,20},{10,-10,20},{-10,-10,20}};
    shape.faces={{-1,0,{0,0,1},{0,1,2,3}},{-1,1,{0,0,1},{4,5,6,7}}};
    shape.colour_words={0x11,0x4000};
    assets::TextureImage texture;texture.descriptor=0x4000;texture.texels={1};
    shape.textures={texture};return shape;
}
}
int main(int argc,char** argv) try {
    vr::set_inset_decals(true); // The Steam Frame rule; the original rule is checked below.
    auto shape=specimen();check(shape,1,true);
    // Coordinates, winding, transforms and polygon corner counts are not identity keys.
    auto reversed=shape;std::reverse(reversed.faces[0].vertex_indices.begin(),reversed.faces[0].vertex_indices.end());
    check(reversed,1,true);
    auto triangle=shape;triangle.faces[1].vertex_indices={4,5,6};check(triangle,1,true);
    auto tilted=shape;for(auto& v:tilted.vertices) v.z=v.x+v.y;check(tilted,1,true);
    auto scaled=shape;scaled.header.shift=3;render::RenderPose pose;pose.scale=.31;
    check(scaled,1,true,pose);
    auto duplicate=shape;
    for(unsigned i=0;i<4;++i) duplicate.vertices[4+i]=duplicate.vertices[i];
    duplicate.faces[1].vertex_indices={7,6,5,4};check(duplicate,1,true);
    auto separate=shape;separate.vertices[4].z+=1;check(separate,1,false);
    for(unsigned i=4;i<8;++i) separate.vertices[i].z=21;check(separate,1,false);
    auto outside=shape;outside.vertices[4].x=-31;check(outside,1,false);
    auto disjoint=shape;for(unsigned i=4;i<8;++i) disjoint.vertices[i].x+=100;check(disjoint,1,false);
    auto edge=shape;edge.vertices[4].x=edge.vertices[7].x=-30;check(edge,1,true);
    auto degenerate=shape;degenerate.vertices[2]=degenerate.vertices[1];degenerate.vertices[3]=degenerate.vertices[0];
    check(degenerate,1,false);
    pose={};pose.explosion_progress=2;check(shape,1,false,pose);
    // A bounding-box test would incorrectly accept these corners over the cutout.
    vr::DecalSurface concave={{{0,0,0}},{{4,0,0}},{{4,1,0}},{{1,1,0}},{{1,4,0}},{{0,4,0}}};
    vr::DecalSurface cutout={{{2,2,0}},{{3,2,0}},{{3,3,0}},{{2,3,0}}};
    require(!vr::decal_surface_contains(concave,cutout));
    vr::DecalSurface backing={{{0,0,0}},{{4,0,0}},{{4,4,0}},{{0,4,0}}};
    require(vr::decal_surface_contains(backing,cutout));
    cutout[0].pose=1;require(!vr::decal_surface_contains(backing,cutout));
    std::cout<<"Synthetic decal CPU/resident regressions passed\n";
    // Everywhere else the original rule stays: only a face repeating a solid face's
    // exact vertices is an overlay, so an inset sign is not.
    vr::set_inset_decals(false);
    check(shape,1,false);check(reversed,1,false);check(triangle,1,false);check(tilted,1,false);
    {render::RenderPose scaled_pose;scaled_pose.scale=.31;check(scaled,1,false,scaled_pose);}
    check(edge,1,false);check(duplicate,1,true);
    require(!vr::decal_surface_contains(backing,cutout) && vr::decal_surface_contains(backing,backing));
    vr::set_inset_decals(true);
    std::cout<<"Original exact-match decal rule passed\n";
    if(argc==3) {
        const auto rom=assets::RomImage::load(argv[1]);const auto symbols=assets::SymbolMap::load(argv[2]);
        assets::ShapeDecoder decoder(rom,symbols);auto tower=decoder.decode_by_name(symbols,"BU_7");
        require(tower.faces.size()==9 && tower.faces[8].vertex_indices.size()==4);
        check(tower,8,true);
        auto displaced=tower;
        for(auto& frame:displaced.frames)
            for(auto index:displaced.faces[8].vertex_indices) frame.vertices[index].z+=1;
        check(displaced,8,false);
        auto protruding=tower;
        for(auto& frame:protruding.frames)
            frame.vertices[protruding.faces[8].vertex_indices[0]].x=-21;
        check(protruding,8,false);
        pose={};pose.explosion_progress=2;check(tower,8,false,pose);
        std::cout<<"Authored BU_7 CPU/resident decal and perturbed negative regressions passed (address 0x"
            <<std::hex<<tower.header.address<<")\n";
    } else require(argc==1);
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
