#include "starfox/render/packed_projection.hpp"
#include "starfox/render/packed_faces.hpp"
#include "starfox/assets/shape_decoder.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <numbers>
#include <set>
#include <stdexcept>
using namespace starfox;
namespace {
void require(bool value){if(!value) throw std::runtime_error("Packed projection assertion failed");}
template<class T> void same(std::span<const T> a,std::span<const T> b) {
    require(a.size()==b.size());
    require(a.empty() || std::memcmp(a.data(),b.data(),a.size_bytes())==0);
}
void prepared(const assets::Shape& shape,render::RenderPose pose,render::RenderSettings settings={}) {
    const render::PreparedProjectionSource source(shape,pose,settings);
    // Pose/camera constants must remain independent from the source packet.
    pose.x-=1.25;pose.y+=2.5;pose.vanish_x+=3.75;pose.roll+=127;
    if(settings.render_scale>1 || pose.continuous_geometry) pose.scale*=1.5;
    const auto reference=render::pack_projection(shape,pose,settings);
    auto actual=render::pack_projection(shape,pose,settings,&source);
    require(actual.source==&source && actual.native_vertices.empty()
        && actual.continuous_vertices.empty() && actual.visibility_faces.empty());
    same(actual.native_input(),reference.native_input());
    same(actual.continuous_input(),reference.continuous_input());
    same(actual.visibility_input(),reference.visibility_input());
    require(std::memcmp(&actual.native_pose,&reference.native_pose,sizeof(actual.native_pose))==0);
    require(std::memcmp(actual.continuous_poses.data(),reference.continuous_poses.data(),sizeof(actual.continuous_poses))==0);
    require(std::memcmp(actual.euler_operands.data(),reference.euler_operands.data(),sizeof(actual.euler_operands))==0);
    actual.own_source();require(!actual.source);
    same(actual.native_input(),reference.native_input());
    same(actual.continuous_input(),reference.continuous_input());
    same(actual.visibility_input(),reference.visibility_input());
    const auto bytes=source.storage_bytes();require(bytes>=source.visibility_input().size_bytes());
}
void inspect(const assets::Shape& shape,std::uint32_t frame) {
    render::RenderPose pose;pose.use_rotation_matrix=true;pose.scale=1.25;pose.animation_frame=frame;
    pose.rotation_matrix={32767,-12345,23456,7890,-32768,19000,-9999,8888,7777};
    const auto& source=shape.frames.empty()?shape.vertices:shape.frames[frame%shape.frames.size()].vertices;
    const auto groups=render::pack_axis_groups(shape,frame);
    for(unsigned group=0;group<2;++group) {
        if(source.empty()) {require(groups[group].empty());continue;}
        require(!groups[group].empty());const auto extreme=source[groups[group].front()].z;
        std::vector<std::uint32_t> expected;
        for(std::size_t i=0;i<source.size();++i) {
            require(group==0?source[i].z<=extreme:source[i].z>=extreme);
            if(source[i].z==extreme) expected.push_back(std::uint32_t(i));
        }
        require(groups[group]==expected);
    }
    const auto& words=shape.frames.empty()?shape.word_coordinates:shape.frames[frame%shape.frames.size()].word_coordinates;
    const double factor=std::ldexp(pose.scale,shape.header.shift);
    auto native=render::pack_projection(shape,pose,{});
    prepared(shape,pose);
    pose.continuous_geometry=true;auto continuous=render::pack_projection(shape,pose,{});
    prepared(shape,pose);pose.use_rotation_matrix=false;prepared(shape,pose);
    require(native.native_vertices.size()==source.size() && continuous.continuous_vertices.size()==source.size());
    for(std::size_t i=0;i<source.size();++i) {
        const bool word=i<words.size() && words[i];const auto& s=source[i];
        const auto& n=native.native_vertices[i];const auto& c=continuous.continuous_vertices[i];
        const auto quantize=[&](int coordinate){return simulation::wrap16(static_cast<std::int64_t>(std::round(coordinate*(word?1.0:factor))));};
        require(n.x==quantize(s.x) && n.y==quantize(s.y) && n.z==quantize(s.z));
        require(c.pose==(word?1U:0U) && c.x==float(s.x) && c.y==float(s.y) && c.z==float(s.z));
    }
    require(native.visibility_faces.size()==continuous.visibility_faces.size());
    for(std::size_t i=0;i<native.visibility_faces.size();++i)
        for(unsigned c=0;c<3;++c) require(native.visibility_faces[i][c]==continuous.visibility_faces[i][c]);
}
}
int main(int argc,char** argv)try {
    {
        assets::Shape axis;
        auto groups=render::pack_axis_groups(axis,0);require(groups[0].empty() && groups[1].empty());
        axis.vertices={{1,2,9},{3,4,-7},{5,6,9},{7,8,-7},{0,0,0}};
        axis.word_coordinates=std::vector<bool>(5,false);axis.word_coordinates[1]=axis.word_coordinates[2]=true;axis.header.shift=7;
        groups=render::pack_axis_groups(axis,0);
        require(groups[0]==std::vector<std::uint32_t>{0,2} && groups[1]==std::vector<std::uint32_t>{1,3});
        axis.frames.resize(2);axis.frames[0].vertices={{1,2,3},{4,5,3}};
        axis.frames[1].vertices={{0,0,-9},{0,0,10},{0,0,10}};
        groups=render::pack_axis_groups(axis,2);
        require(groups[0]==std::vector<std::uint32_t>{0,1} && groups[1]==groups[0]);
        groups=render::pack_axis_groups(axis,3);
        require(groups[0]==std::vector<std::uint32_t>{1,2} && groups[1]==std::vector<std::uint32_t>{0});
    }
    assets::Shape shape;shape.header.shift=2;shape.vertices={{1,-1,32767},{1,-1,32767}};shape.word_coordinates=std::vector<bool>(2,false);shape.word_coordinates[1]=true;
    shape.visibilities={{0,1,255}};inspect(shape,0);
    render::RenderPose pose;pose.use_rotation_matrix=true;pose.scale=.125;pose.rotation_matrix={32767,0,0,0,32767,0,0,0,32767};
    pose.x=-.5;pose.y=.5;pose.vanish_x=32768;
    auto packed=render::pack_projection(shape,pose,{});
    require(packed.native_vertices[0].x==1 && packed.native_vertices[0].y==-1 && packed.native_vertices[0].z==16384);
    require(packed.native_vertices[1].z==32767 && packed.native_pose.translation[0]==-1 && packed.native_pose.translation[1]==1);
    require(packed.native_pose.vanish[0]==-32768 && packed.visibility_faces[0][2]==255);
    pose.continuous_geometry=true;packed=render::pack_projection(shape,pose,{});
    require(packed.continuous_poses[0].row0[0]==32767.0F/65536 && packed.continuous_poses[1].row0[0]==32767.0F/32768);
    pose.use_rotation_matrix=false;pose.yaw=16384;packed=render::pack_projection(shape,pose,{});
    require(std::abs(packed.continuous_poses[1].row0[2]+1)<1e-6 && std::abs(packed.continuous_poses[1].row2[0]-1)<1e-6);
    shape.frames={{{},{{2,3,4}},{false}},{{},{{5,6,7}},{true}}};inspect(shape,3);
    shape.header.shift=255;pose.animation_frame=1;
    packed=render::pack_projection(shape,pose,{});require(packed.continuous_vertices[0].pose==1);
    shape.header.shift=2;pose.animation_frame=0;
    assets::Shape line;line.vertices={{0,-2,0},{0,-1,0},{0,-60,0}};line.visibilities={{0,1,2},{0,1,9}};
    render::RenderPose line_pose;line_pose.continuous_geometry=true;line_pose.yaw=16384;line_pose.pitch=8192;
    auto line_packet=render::pack_projection(line,line_pose,{});
    require(line_packet.continuous_poses[0].vanish[3]==1);
    const double coefficient=double(line_packet.continuous_poses[1].row1[1])+line_packet.continuous_poses[3].row1[1];
    require(std::abs(coefficient-std::sqrt(.5))<1e-14);
    const auto& eh=line_packet.euler_operands[0];const auto& el=line_packet.euler_operands[1];
    require(std::abs(double(eh.row0[0])+el.row0[0]-std::sqrt(.5))<1e-14);
    require(std::abs(double(eh.row1[0])+el.row1[0]-std::sqrt(.5))<1e-14);
    require(eh.row1[1]==1.f && eh.row2[1]==1.f && el.row2[1]==0.f);
    for(unsigned angle=0;angle<65536;angle+=127) {
        auto sample_pose=line_pose;sample_pose.pitch=angle;sample_pose.yaw=(angle+8192)%65536;sample_pose.roll=(angle+16384)%65536;
        const auto sample=render::pack_projection(line,sample_pose,{});
        const auto& high=sample.euler_operands[0];const auto& low=sample.euler_operands[1];
        const double angles[]{sample_pose.pitch*2*std::numbers::pi/65536.,
            sample_pose.yaw*2*std::numbers::pi/65536.,sample_pose.roll*2*std::numbers::pi/65536.};
        for(unsigned c=0;c<3;++c) {
            require((double(high.row0[c])+double(low.row0[c]))+double(high.translation[c])==std::cos(angles[c]));
            require((double(high.row1[c])+double(low.row1[c]))+double(low.translation[c])==std::sin(angles[c]));
        }
    }
    require(line_packet.visibility_faces[0][3]==2 && line_packet.visibility_faces[1][3]==0);
    line.word_coordinates=std::vector<bool>(3,false);line.word_coordinates[1]=true;
    require(render::pack_projection(line,line_pose,{}).visibility_faces[0][3]==0);
    line_pose.continuous_geometry=false;line_pose.use_rotation_matrix=true;
    require(render::pack_projection(line,line_pose,{}).visibility_faces[0][3]==0);
    bool threw=false;pose.scale=std::numeric_limits<double>::infinity();
    try{static_cast<void>(render::pack_projection(shape,pose,{}));}catch(const std::runtime_error&){threw=true;}require(threw);
    pose={};threw=false;try{static_cast<void>(render::pack_projection(shape,pose,{}));}catch(const std::runtime_error&){threw=true;}require(threw);
    {
        render::RenderPose p;p.continuous_geometry=true;p.animation_frame=0;
        const render::PreparedProjectionSource source(shape,p,{});
        auto bad_shape=shape;
        for(unsigned policy=0;policy<3;++policy) {
            auto other=p;render::RenderSettings settings;
            if(policy==1) other.animation_frame=1;
            if(policy==2) {other.continuous_geometry=false;other.use_rotation_matrix=true;}
            bool rejected=false;
            try{static_cast<void>(render::pack_projection(policy==0?bad_shape:shape,other,settings,&source));}
            catch(const std::runtime_error&){rejected=true;}require(rejected);
        }
        // Native prescaled bytes cannot be reused at a different object scale.
        p.continuous_geometry=false;p.use_rotation_matrix=true;
        const render::PreparedProjectionSource native(shape,p,{});p.scale=2;
        require(!native.matches(shape,p,{}));
        // All-word native coordinates do not depend on unused object scale,
        // even with a header shift which would be illegal for byte vertices.
        auto word=shape;word.header.shift=255;
        p.animation_frame=1;p.scale=std::numeric_limits<double>::quiet_NaN();
        const render::PreparedProjectionSource word_source(word,p,{});
        require(word_source.matches(word,p,{}));
        same(render::pack_projection(word,p,{},&word_source).native_input(),word_source.native_input());
    }
    {
        assets::Shape fragment;fragment.header.shift=1;
        fragment.vertices={{-10,-10,0},{10,-10,0},{0,10,0}};
        fragment.word_coordinates=std::vector<bool>(3,false);fragment.word_coordinates[1]=true;
        fragment.faces={{-1,0,{1,2,3},{2,1,0}}};
        render::RenderPose p;p.continuous_geometry=true;p.explosion_progress=7;p.explosion_phase=6.5;
        p.force_colour=true;p.forced_colour=0x11;
        const render::PreparedProjectionSource source(fragment,p,{});
        const auto graph=render::pack_bsp(fragment,true);
        auto reference=render::pack_projection(fragment,p,{}),actual=render::pack_projection(fragment,p,{},&source);
        auto ref_faces=render::pack_faces(fragment,graph,p,{}),actual_faces=ref_faces;
        const auto ref_poses=render::pack_continuous_fragments(reference,ref_faces,graph,p,{},false);
        const auto actual_poses=render::pack_continuous_fragments(actual,actual_faces,graph,p,{},false);
        require(!actual.source && source.continuous_input().size()==3);
        same(actual.continuous_input(),reference.continuous_input());
        same(actual.visibility_input(),reference.visibility_input());
        same<render::ContinuousTransformPose>(actual_poses,ref_poses);
        same<std::array<std::uint32_t,4>>(actual_faces.corners,ref_faces.corners);
    }
    unsigned models=0,frames=0;
    if(argc==3) {
        const auto rom=assets::RomImage::load(argv[1]);const auto symbols=assets::SymbolMap::load(argv[2]);const assets::ShapeDecoder decoder(rom,symbols);
        std::set<std::uint32_t> seen;
        for(const auto& [name,addresses]:symbols.entries()) for(auto address:addresses)
            if(seen.insert(address).second && decoder.looks_like_shape_header(address)) {
                const auto model=decoder.decode(address,name);++models;
                for(unsigned frame=0;frame<std::max(std::size_t(1),model.frames.size());++frame){inspect(model,frame);++frames;}
            }
    } else require(argc==1);
    std::cout<<"Projection packing passed: "<<models<<" decoded models, "<<frames<<" frames, native/continuous word scaling\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
