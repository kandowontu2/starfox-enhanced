#include "starfox/vr/source_ray_topology.hpp"
#include <iostream>
#include <stdexcept>
int main() {
    try {
        const auto require=[](bool value){if(!value) throw std::runtime_error("Ray topology assertion failed");};
        starfox::render::PackedFaces faces;
        faces.corners={{2,0,0,0},{0,0,0,0},{3,0,0,0},{1,0,0,0},{4,0,0,0},{5,0,0,0}};
        faces.polygons={{0,4,999,8},{4,2,0,2}};
        faces.primitives={starfox::render::PackedPrimitive::polygon,starfox::render::PackedPrimitive::line};
        std::vector<std::array<uint32_t,4>> result;
        std::string error;
        require(starfox::vr::source_ray_topology(faces,6,result,error));
        require(result==std::vector<std::array<uint32_t,4>>{{0,1,2,0},{0,2,3,0}});
        const auto saved=result;
        faces.corners[2][0]=6;
        require(!starfox::vr::source_ray_topology(faces,6,result,error) && result==saved);
        faces.corners[2][0]=3;faces.polygons[0][0]=UINT32_MAX;
        require(!starfox::vr::source_ray_topology(faces,6,result,error) && result==saved);
        faces.polygons[0][0]=0;faces.polygons[0][1]=2;
        require(!starfox::vr::source_ray_topology(faces,6,result,error) && result==saved);
        faces={};require(starfox::vr::source_ray_topology(faces,0,result,error) && result.empty() && error.empty());
        std::cout<<"Unculled ray topology, fan order, provenance and transactional rejection passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
