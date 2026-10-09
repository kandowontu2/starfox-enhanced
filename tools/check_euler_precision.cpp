#include "starfox/assets/shape_decoder.hpp"
#include "starfox/render/packed_projection.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numbers>
int main(int argc,char** argv) {
    if(argc<3 || argc>5) return 2;
    const auto rom=starfox::assets::RomImage::load(argv[1]);const auto symbols=starfox::assets::SymbolMap::load(argv[2]);
    const starfox::assets::ShapeDecoder decoder(rom,symbols);const auto shape=decoder.decode_by_name(symbols,argc>=4?argv[3]:"IRIS");
    starfox::render::RenderPose pose;pose.pitch=8192;pose.yaw=16384;pose.x=11;pose.y=-4;pose.z=512;pose.continuous_geometry=true;
    if(argc==4) {pose.use_rotation_matrix=true;pose.rotation_matrix={0,0,-32768,0,-32768,0,32767,0,0};pose.x=.25;pose.y=-.125;pose.z=256;}
    if(argc==5) {pose.x=-6.75;pose.y=2.875;}
    const auto packed=starfox::render::pack_projection(shape,pose,{});
    const auto& vertices=shape.frames.empty()?shape.vertices:shape.frames[0].vertices;
    const auto& words=shape.frames.empty()?shape.word_coordinates:shape.frames[0].word_coordinates;
    const double pitch=pose.pitch*(2*std::numbers::pi)/65536.,yaw=pose.yaw*(2*std::numbers::pi)/65536.;
    std::cout<<std::setprecision(17);
    std::vector<std::array<double,3>> source_points,float_points;
    for(std::size_t i=0;i<vertices.size();++i) {
        const auto& v=vertices[i];const double scale=i<words.size() && words[i]?1.:std::ldexp(pose.scale,shape.header.shift);
        double x=v.x*scale,y=v.y*scale,z=v.z*scale;
        const double y1=y*std::cos(pitch)-z*std::sin(pitch),z1=y*std::sin(pitch)+z*std::cos(pitch);
        const double x2=x*std::cos(yaw)+z1*std::sin(yaw),z2=-x*std::sin(yaw)+z1*std::cos(yaw);
        double camera[]{x2+pose.x,y1+pose.y,z2+pose.z};
        if(pose.use_rotation_matrix) for(unsigned c=0;c<3;++c) camera[c]=(x*pose.rotation_matrix[c]+y*pose.rotation_matrix[3+c]+z*pose.rotation_matrix[6+c])/32768.+(c==0?pose.x:c==1?pose.y:pose.z);
        const auto& pv=packed.continuous_vertices[i];const auto& p=packed.continuous_poses[pv.pose];
        float gpu[3];for(unsigned c=0;c<3;++c) gpu[c]=pv.x*p.row0[c]+pv.y*p.row1[c]+pv.z*p.row2[c]+p.translation[c];
        source_points.push_back({camera[0],camera[1],camera[2]});float_points.push_back({gpu[0],gpu[1],gpu[2]});
        for(unsigned axis=0;axis<2;++axis) {
            const double source=p.vanish[axis]+camera[axis]*256./camera[2];
            const float actual=p.vanish[axis]+gpu[axis]*256.F/gpu[2];
            std::cout<<"vertex "<<i<<" axis "<<axis<<" source="<<source<<" float="<<actual
                <<" rounded="<<std::lround(source)<<'/'<<std::lround(actual)<<" 2x="<<std::lround(source*2)<<'/'<<std::lround(actual*2)<<'\n';
        }
    }
    for(std::size_t i=0;i<shape.visibilities.size();++i) {
        const auto& face=shape.visibilities[i];
        const auto decision=[&](const auto& points) {
            const auto a=points.at(face.a),b=points.at(face.b),c=points.at(face.c);
            const double determinant=a[0]*(b[1]*c[2]-b[2]*c[1])-a[1]*(b[0]*c[2]-b[2]*c[0])+a[2]*(b[0]*c[1]-b[1]*c[0]);
            double scale=1;for(auto v:{a,b,c}) for(auto coordinate:v) scale=std::max(scale,std::abs(coordinate));
            return std::array<double,2>{determinant,scale*scale*scale*1e-12};
        };
        const auto a=decision(source_points),b=decision(float_points);
        if((a[0]<=a[1])!=(b[0]<=b[1])) {
            std::cout<<"VISIBILITY MISMATCH "<<i<<" source "<<a[0]<<" <= "<<a[1]<<" float "<<b[0]<<" <= "<<b[1]<<'\n';
            for(auto index:{face.a,face.b,face.c}) {const auto& v=vertices[index];std::cout<<unsigned(index)<<": "<<v.x<<','<<v.y<<','<<v.z<<'\n';}
        }
    }
}
